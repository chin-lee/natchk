#include "longopt.h"
#include "log.h"
#include "util.h"
#include "async.h"
#include "endpoint.h"
#include "message.h"
#include "udpsvc.h"
#include <thread>

static const option_t kOptions[] = {
    { '-', NULL, 0, NULL, "arguments:" },
    { 'l', "listen-udp", LONGOPT_REQUIRE, NULL, "<ip>:<port>,<ip>:<port>,..."},
    { 0, NULL, 0, NULL, NULL }
};

class Server : public UdpService::IMessageHandler {
    uv_loop_t& m_loop;
    UdpService m_udpSvc;

    static std::vector<Server*> s_servers;

public:
    Server(uv_loop_t& loop, const Endpoint& listenAddr) 
        : m_loop(loop), m_udpSvc(loop, listenAddr) {
        m_udpSvc.addMessageHandler(this);
        m_udpSvc.start();
        s_servers.push_back(this);
    }

    void handleMessage(UdpService& udpSvc, const Endpoint& peer, 
                       const char* data, int size) override {
        MessageId msgId = MessageId(data[0]);
        switch (msgId) {
        case MessageId::GETADDR:
            LOGD << "recv GETADDR from " << peer.ip() << ":" << peer.port();
            sendAddr(peer);
            break;
        case MessageId::CHKFULLCONE:
            LOGD << "recv CHKFULLCONE from " << peer.ip() << ":" << peer.port();
            onCheckFullCone(peer, data, size);
            break;
        case MessageId::SENDFULLCONE:
            LOGD << "recv SENDFULLCONE from " << peer.ip() 
                 << ":" << peer.port();
            onSendFullCone(data, size);
            break;
        case MessageId::CHKRESTRICTEDCONE:
            LOGD << "recv CHKRESTRICTEDCONE from " << peer.ip() 
                 << ":" << peer.port();
            onCheckRestrictedCone(peer);
        default:
            break;
        }
    }

private:
    void sendAddr(const Endpoint& peer) {
        char buf[1 + sizeof(struct sockaddr_in6)];
        memset(buf, 0, sizeof(buf));
        buf[0] = char(MessageId::ADDR);
        int size = peer.serializeToArray(buf + 1, sizeof(struct sockaddr_in6));
        m_udpSvc.send(peer, buf, 1 + size);
    }

    void onCheckFullCone(const Endpoint& peer, const char* data, int size) {
        Endpoint anotherSvr((const struct sockaddr*)(data + 1));
        char buf[1 + sizeof(struct sockaddr_in6)];
        memset(buf, 0, sizeof(buf));
        buf[0] = char(MessageId::SENDFULLCONE);
        int len = peer.serializeToArray(buf + 1, sizeof(struct sockaddr_in6));
        m_udpSvc.send(anotherSvr, buf, 1 + len);
        LOGD << "send SENDFULLCONE to " << anotherSvr.ip() 
             << ":" << anotherSvr.port();
    }

    void onSendFullCone(const char* data, int size) {
        Endpoint endpoint((const struct sockaddr*)(data + 1));
        char buf[1];
        buf[0] = char(MessageId::FULLCONE);
        m_udpSvc.send(endpoint, buf, 1);
        LOGD << "send FULLCONE to " << endpoint.ip() << ":" << endpoint.port();
    }

    void onCheckRestrictedCone(const Endpoint& peer) {
        for (Server* svr : s_servers) {
            if (svr != this) {
                char buf[1];
                buf[0] = char(MessageId::RESTRICTEDCONE);
                svr->m_udpSvc.send(peer, buf, 1);
                LOGD << "send RESTRICTEDCONE to " << peer.ip() 
                     << ":" << peer.port();
                break;
            }
        }
    }
};

std::vector<Server*> Server::s_servers;

int main(int argc, char* argv[]) {
    std::string listenAddrListStr;
    int opt;
    while ( (opt = longopt(argc, argv, kOptions)) != LONGOPT_DONE ) {
        if (LONGOPT_NEED_PARAM == opt) {
            const option_t& ent = kOptions[errindex];
            LOGE << "missing parameter for -" << char(ent.val) 
                 << "--" << ent.name;
            exit(1);
        } else if (opt <= 0 || opt >= (int)ARRAY_SIZE(kOptions)) {
            continue;
        }
        switch (opt) {
        case 1:
            listenAddrListStr = optparam;
            break;
        }
    }

    if (listenAddrListStr.empty()) {
        print_opt(kOptions);
        return 1;
    }

    std::vector<IpPort> listenAddrList;
    if (!util::parseIpPortList(listenAddrListStr, listenAddrList)) {
        LOGE << "invalid argument " << listenAddrListStr;
        return 1;
    }

    uv_loop_t mainloop;
    uv_loop_init(&mainloop);

    AsyncHandler handler(mainloop);

    std::thread t([&mainloop]() {
        uv_run(&mainloop, UV_RUN_DEFAULT);
    });

    handler.post([&]() {
        for (const IpPort& addr : listenAddrList) {
            Endpoint endpoint(AF_INET, addr.ip, addr.port);
            new Server(mainloop, endpoint);
        }
    });

    t.join();
    uv_loop_close(&mainloop);

    return 0;
}
