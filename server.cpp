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

class MessageHandler : public UdpService::IMessageHandler {
public:
     void handleMessage(UdpService& udpSvc, const Endpoint& peer, 
                        const char* data, int size) override {
        MessageId msgId = MessageId(data[0]);
        switch (msgId) {
        case MessageId::GETADDR:
            {
                LOGD << "recv GETADDR from " << peer.ip() << ":" << peer.port();
                char buf[1 + sizeof(struct sockaddr_in6)];
                memset(buf, 0, sizeof(buf));
                buf[0] = char(MessageId::ADDR);
                int size = peer.serializeToArray(
                        buf + 1, sizeof(struct sockaddr_in6));
                udpSvc.send(peer, buf, 1 + size);
            }
            break;
        default:
            break;
        }
    }
};

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

    MessageHandler* msgHandler = new MessageHandler;
    std::vector<UdpService*> udpSvcList;
    handler.post([&]() {
        for (const IpPort& addr : listenAddrList) {
            Endpoint endpoint(AF_INET, addr.ip, addr.port);
            UdpService* udpSvc = new UdpService(mainloop, endpoint);
            udpSvc->start();
            udpSvc->addMessageHandler(msgHandler);
            udpSvcList.push_back(udpSvc);
        }
    });

    t.join();
    uv_loop_close(&mainloop);

    return 0;
}
