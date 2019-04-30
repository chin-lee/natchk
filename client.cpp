#include "longopt.h"
#include "log.h"
#include "util.h"
#include "async.h"
#include "endpoint.h"
#include "message.h"
#include <uv.h>
#include <arpa/inet.h>
#include <string>
#include <vector>
#include <thread>
#include <stdlib.h>

struct SendReq {
    uv_udp_send_t handle;
    Endpoint peer;
    int size;
    char data[1];
};

static const option_t kOptions[] = {
    { '-', NULL, 0, NULL, "arguments:" },
    { 's', "servers", LONGOPT_REQUIRE, NULL, "udp server list <ip>:<port>,<ip>:<port>,..." },
    { 0, NULL, 0, NULL, NULL }
};

static bool udpInit(uv_loop_t& loop, uv_udp_t& handle) {
    int retval = uv_udp_init(&loop, &handle);
    if (retval < 0) {
        LOGE << "uv_udp_init: " << uv_strerror(retval);
        return false;
    }
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = 10336;
    retval = uv_udp_bind(&handle, 
        (const struct sockaddr*)&addr, 
        UV_UDP_REUSEADDR);
    if (retval < 0) {
        LOGE << "uv_udp_bind: " << uv_strerror(retval);
        return false;
    }
    return true;
}

static void handleSend(uv_udp_send_t* req, int status) {
    SendReq* sr = CONTAINER_OF(req, SendReq, handle);
    LOGI << "sent " << sr->size << " byte(s) to " << sr->peer.ip() 
         << ":" << sr->peer.port() << ": " << uv_strerror(status);
    free(sr);
}

static bool udpSend(uv_udp_t& handle, const struct sockaddr* addr, 
                    const char* data, int size) {
    SendReq* req = (SendReq*)malloc(sizeof(SendReq) + size);
    new(&req->peer) Endpoint(addr);
    req->size = size;
    memcpy(req->data, data, size);
    uv_buf_t bufs[1];
    bufs[0].base = req->data;
    bufs[0].len = req->size;
    int retval = uv_udp_send(&req->handle, &handle, bufs, 1, addr, handleSend);
    if (retval < 0) {
        LOGE << "uv_udp_send: " << uv_strerror(retval);
        return false;
    }
    return true;
}

int main(int argc, char* argv[]) {
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
            svrAddrListStr = optparam;
            break;
        }
    }

    if (svrAddrListStr.empty()) {
        print_opt(kOptions);
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

    uv_udp_t udphandle;
    handler.post([&]() {
        if (!udpInit(mainloop, udphandle)) {
            return;
        }
        for (const IpPort& ipPort : svrAddrList) {
            Endpoint ep(AF_INET, ipPort.ip, ipPort.port);
            char buf[1];
            buf[0] = (char)MESSAGE_TYPE_PING;
            udpSend(udphandle, ep.addr(), buf, sizeof(buf));
        }
    });
    handler.shutdown();

    t.join();
    uv_loop_close(&mainloop);

    return 0;
}