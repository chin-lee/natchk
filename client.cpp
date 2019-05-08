#include "longopt.h"
#include "log.h"
#include "util.h"
#include "async.h"
#include "endpoint.h"
#include "message.h"
#include "udpsvc.h"
#include <uv.h>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <stdlib.h>

static const option_t kOptions[] = {
    { '-', NULL, 0, NULL, "arguments:" },
    { 'l', "listen-udp", LONGOPT_REQUIRE, NULL, "<ip>:<port>"},
    { 's', "servers", LONGOPT_REQUIRE, NULL, "udp server list <ip>:<port>,<ip>:<port>,..." },
    { 0, NULL, 0, NULL, NULL }
};

static const int kGetAddrIntervalMillis = 2000;
static const int kMaxGetAddrCount = 5;

class Client;

struct GetAddrTask {
    Client& m_client;
    Endpoint m_svr;
    uv_timer_t m_timer;
    int m_tryCount;

    static void onTimeout(uv_timer_t* handle) {
        GetAddrTask* self = CONTAINER_OF(handle, GetAddrTask, m_timer);
        if (self->m_tryCount < kMaxGetAddrCount) {
            self->send();
            self->m_tryCount += 1;
        } else {
            LOGW << "failed to get address from " << self->m_svr.ip() 
                 << ":" << self->m_svr.port();
            self->stop();
        }
    }

    static void onCloseHandle(uv_handle_t* handle) {
        GetAddrTask* self = CONTAINER_OF(handle, GetAddrTask, m_timer);
        delete self;
    }

    GetAddrTask(Client& client, const Endpoint& svr);
    void send();
    void stop();
};

class Client : public UdpService::IMessageHandler {
    friend class GetAddrTask;

    uv_loop_t& m_loop;
    UdpService m_udpSvc;
    std::vector<IpPort> m_svrList;

    typedef std::map<Endpoint, GetAddrTask*> GetAddrTaskMap;
    GetAddrTaskMap m_getAddrTaskMap;

    typedef std::map<Endpoint, Endpoint> AddrMap;
    AddrMap m_addrMap;

public:
    Client(uv_loop_t& loop, const Endpoint& listenAddr, 
           const std::vector<IpPort>& svrList)
        : m_loop(loop), m_udpSvc(loop, listenAddr), m_svrList(svrList) {
        m_udpSvc.addMessageHandler(this);
        m_udpSvc.start();
        for (const IpPort& addr : m_svrList) {
            Endpoint endpoint(AF_INET, addr.ip, addr.port);
            getAddr(endpoint);
        }
    }

    void handleMessage(UdpService& udpSvc, const Endpoint& peer, 
                       const char* data, int size) override {
        MessageId msgId = MessageId(data[0]);
        switch (msgId) {
        case MessageId::ADDR:
            {
                const struct sockaddr* sa = 
                        (const struct sockaddr*)(data + 1);
                Endpoint me(sa);
                LOGI << "recv ADDR from " << peer.ip() << ":" << peer.port() 
                     << ", my address is " << me.ip() << ":" << me.port();
                auto it = m_getAddrTaskMap.find(peer);
                if (it != m_getAddrTaskMap.end()) {
                    m_addrMap.emplace(peer, me);
                    it->second->stop();
                }
            }
            break;
        default:
            LOGW << "unknown message " << int(msgId) << " recvd";
            break;
        }
    }

private:
    void getAddr(const Endpoint& svr) {
        GetAddrTask* task = new GetAddrTask(*this, svr);
        m_getAddrTaskMap.emplace(svr, task);
    }
};

GetAddrTask::GetAddrTask(Client& client, const Endpoint& svr)
    : m_client(client), m_svr(svr), m_tryCount(0) {
    uv_timer_init(&client.m_loop, &m_timer);
    uv_timer_start(&m_timer, onTimeout, 0, kGetAddrIntervalMillis);
}

void GetAddrTask::send() {
    LOGD << "send GETADDR to " << m_svr.ip() << ":" << m_svr.port();
    char buf[1];
    buf[0] = char(MessageId::GETADDR);
    m_client.m_udpSvc.send(m_svr, buf, 1);
}

void GetAddrTask::stop() {
    uv_timer_stop(&m_timer);
    uv_close((uv_handle_t*)&m_timer, onCloseHandle);
    m_client.m_getAddrTaskMap.erase(m_svr);
}

int main(int argc, char* argv[]) {
    std::string listenAddrStr;
    std::string svrAddrListStr;
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
            listenAddrStr = optparam;
            break;
        case 2:
            svrAddrListStr = optparam;
            break;
        }
    }

    if (listenAddrStr.empty()) {
        print_opt(kOptions);
        return 1;
    } else if (svrAddrListStr.empty()) {
        print_opt(kOptions);
        return 1;
    }

    IpPort listenAddr;
    if (!util::parseIpPort(listenAddrStr, listenAddr)) {
        LOGE << "invalid argument " << listenAddrStr;
        return 1;
    }

    std::vector<IpPort> svrAddrList;
    if (!util::parseIpPortList(svrAddrListStr, svrAddrList)) {
        LOGE << "invalid argument " << svrAddrListStr;
        return 1;
    }

    uv_loop_t mainloop;
    uv_loop_init(&mainloop);

    AsyncHandler handler(mainloop);

    std::thread t([&mainloop]() {
        uv_run(&mainloop, UV_RUN_DEFAULT);
    });

    handler.post([&]() {
        Endpoint endpoint(AF_INET, listenAddr.ip, listenAddr.port);
        new Client(mainloop, endpoint, svrAddrList);
    });
    handler.shutdown();

    t.join();
    uv_loop_close(&mainloop);

    return 0;
}
