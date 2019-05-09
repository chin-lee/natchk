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
#include <set>
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

struct InterfaceAddress {
    std::string name;
    Endpoint addr4;
    Endpoint addr6;
};

struct CheckSymmetricNatContext {
    std::vector<Endpoint> myAddrList;
    int finishedTasks;
};

class Client;
// -----------------------------------------------------------------------------
// Section: GetAddrTask
// -----------------------------------------------------------------------------
class GetAddrTask : public UdpService::IMessageHandler {
    typedef std::function<void(const Endpoint*)> CompletionHandler;

    Client& m_client;
    Endpoint m_svr;
    uv_timer_t m_timer;
    int m_tryCount;
    CompletionHandler m_completionHandler;

public:
    static void onTimeout(uv_timer_t* handle);
    static void onCloseHandle(uv_handle_t* handle);

    GetAddrTask(Client& client, const Endpoint& svr, 
                CompletionHandler&& handler);

private:
    void handleMessage(UdpService& udpSvc, const Endpoint& peer, 
                       const char* data, int size) override;
    void send();
    void stop();
};

// -----------------------------------------------------------------------------
// Section: Client
// -----------------------------------------------------------------------------
class Client : public UdpService::IMessageHandler {
    friend class GetAddrTask;

    uv_loop_t& m_loop;
    UdpService m_udpSvc;
    std::vector<IpPort> m_svrList;

    typedef std::map<std::string, InterfaceAddress> InterfaceMap;
    InterfaceMap m_interfaceMap;

public:
    Client(uv_loop_t& loop, const Endpoint& listenAddr, 
           const std::vector<IpPort>& svrList)
        : m_loop(loop), m_udpSvc(loop, listenAddr), m_svrList(svrList) {
        m_udpSvc.addMessageHandler(this);
        m_udpSvc.start();
        queryInterfaceAddresses();
        checkIfBehindNat();
    }

    void handleMessage(UdpService& udpSvc, const Endpoint& peer, 
                       const char* data, int size) override {
    }

private:
    void queryInterfaceAddresses() {
        uv_interface_address_t* addrs;
        int count = 0;
        int retval = uv_interface_addresses(&addrs, &count);
        if (retval != 0) {
            LOGE << "uv_interface_addresses: " << uv_strerror(retval);
            return;
        }
        for (int i = 0; i < count; i++) {
            uv_interface_address_t& entry = addrs[i];
            std::string name = entry.name;
            Endpoint endpoint((const struct sockaddr*)&entry.address);
            InterfaceAddress& ia = m_interfaceMap[name];
            ia.name = name;
            int af = endpoint.sockaddr()->sa_family;
            if (AF_INET == af) {
                ia.addr4 = endpoint;
            } else if (AF_INET6 == af) {
                ia.addr6 = endpoint;
            } else {
                LOGW << "unsupported address family " << af 
                     << " found on interface " << name 
                     << ", ignoring";
                continue;
            }
        }
        uv_free_interface_addresses(addrs, count);
        LOGI << "found " << m_interfaceMap.size() << " interface(s)";
        for (auto it = m_interfaceMap.begin(); 
                it != m_interfaceMap.end(); ++it) {
            const InterfaceAddress& ia = it->second;
            LOGI << ia.name 
                 << ", v4: " << (ia.addr4.v4() ? ia.addr4.ip() : "") 
                 << ", v6: " << (ia.addr6.v6() ? ia.addr6.ip() : "");
        }
    }

    void checkIfBehindNat() {
        LOGI << "check if behind NAT";
        const IpPort& addr = m_svrList[0];
        Endpoint endpoint(AF_INET, addr.ip, addr.port);
        new GetAddrTask(*this, endpoint, [this](const Endpoint* myAddr) {
            if (nullptr == myAddr) {
                return;
            }
            bool behindNat = true;
            for (auto it = m_interfaceMap.begin(); 
                    it != m_interfaceMap.end(); ++it) {
                const InterfaceAddress& ia = it->second;
                if (*myAddr == ia.addr4 || *myAddr == ia.addr6) {
                    behindNat = false;
                    break;
                }
            }
            if (!behindNat) {
                LOGI << "host has public ip address!";
            } else {
                LOGI << "host may behind NAT!";
                checkIfSymmetricNat();
            }
        });
    }

    void checkIfFullConeNat() {

    }

    void checkIfSymmetricNat() {
        if (m_svrList.size() < 2) {
            LOGW << "you must specify more than TWO servers with public IP "
                    "address for checking symmetric NAT";
            return;
        }
        LOGI << "check symmetric NAT";
        CheckSymmetricNatContext* ctx = new CheckSymmetricNatContext;
        ctx->myAddrList.reserve(m_svrList.size());
        ctx->finishedTasks = 0;
        for (const IpPort& addr : m_svrList) {
            Endpoint endpoint(AF_INET, addr.ip, addr.port);
            new GetAddrTask(*this, endpoint, [this, ctx](
                                                const Endpoint* myAddr) {
                ctx->finishedTasks += 1;
                if (nullptr != myAddr) {
                    ctx->myAddrList.emplace_back(*myAddr);
                }
                if (ctx->finishedTasks == m_svrList.size()) {
                    std::map<std::string, std::set<uint16_t>> ipPorts;
                    for (const Endpoint& ep : ctx->myAddrList) {
                        std::set<uint16_t>& portSet = ipPorts[ep.ip()];
                        portSet.insert(ep.port());
                        if (portSet.size() >= 2) {
                            LOGI << "Symmetric NAT!";
                        }
                    }
                    delete ctx;
                }
            });
        }
    }
};

// -----------------------------------------------------------------------------
// Section: GetAddrTask implementation
// -----------------------------------------------------------------------------
// static
void GetAddrTask::onTimeout(uv_timer_t* handle) {
    GetAddrTask* self = CONTAINER_OF(handle, GetAddrTask, m_timer);
    if (self->m_tryCount < kMaxGetAddrCount) {
        self->send();
        self->m_tryCount += 1;
    } else {
        LOGW << "failed to get address from " << self->m_svr.ip() 
             << ":" << self->m_svr.port();
        self->m_completionHandler(nullptr);
        self->stop();
    }
}

// static
void GetAddrTask::onCloseHandle(uv_handle_t* handle) {
    GetAddrTask* self = CONTAINER_OF(handle, GetAddrTask, m_timer);
    delete self;
}

GetAddrTask::GetAddrTask(Client& client, const Endpoint& svr,
                         CompletionHandler&& handler)
    : m_client(client), m_svr(svr), m_tryCount(0)
    , m_completionHandler(std::move(handler)) {
    uv_timer_init(&client.m_loop, &m_timer);
    uv_timer_start(&m_timer, onTimeout, 0, kGetAddrIntervalMillis);
    m_client.m_udpSvc.addMessageHandler(this);
}

void GetAddrTask::handleMessage(UdpService& udpSvc, const Endpoint& peer, 
                                const char* data, int size) {
    MessageId msgId = MessageId(data[0]);
    if ( (MessageId::ADDR == msgId) && (peer == m_svr) ) {
        const struct sockaddr* sa = (const struct sockaddr*)(data + 1);
        Endpoint myAddr(sa);
        LOGI << "recv ADDR from " << peer.ip() << ":" << peer.port() 
             << ", my address is " << myAddr.ip() << ":" << myAddr.port();
        m_completionHandler(&myAddr);
        stop();
    }
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
    m_client.m_udpSvc.removeMessageHandler(this);
}

// -----------------------------------------------------------------------------
// Section: main
// -----------------------------------------------------------------------------
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
        LOGD << "mainloop exied";
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
