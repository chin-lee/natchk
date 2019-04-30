#include "longopt.h"
#include "log.h"
#include "util.h"
#include "async.h"
#include "endpoint.h"
#include "message.h"
#include <thread>

static const option_t kOptions[] = {
    { '-', NULL, 0, NULL, "arguments:" },
    { 'l', "listen-udp", LONGOPT_REQUIRE, NULL, "<ip>:<port>"},
    { 0, NULL, 0, NULL, NULL }
};

static void handleAlloc(uv_handle_t* handle, 
                        size_t suggested_size, 
                        uv_buf_t* buf) {
    buf->base = (char*)malloc(suggested_size);
    buf->len = suggested_size;
    (void)handle;
}

static void handleRecv(uv_udp_t* handle, ssize_t nread, 
                           const uv_buf_t* buf, 
                           const struct sockaddr* addr, 
                           unsigned flags) {
    if (0 == nread && NULL == addr) {
        free(buf->base);
        return;
    }
    Endpoint peer(addr);
    LOGI << "recv " << nread << " byte(s) from " << peer.ip() 
         << ":" << peer.port();
    if ( (flags & UV_UDP_PARTIAL) != 0 ) {
        LOGE << "partial data received from " << peer.ip() << ":" 
                << peer.port();
        free(buf->base);
        return;
    }
    if (nread > 0) {
        int msgType = buf->base[0];
        switch (msgType) {
        case MESSAGE_TYPE_PING:
            LOGI << "recv PING from " << peer.ip() << ":" << peer.port();
            break;
        default:
            LOGI << "recv unknown message from " << peer.ip() 
                 << ":" << peer.port();
        }
    }
}

static bool udpInit(uv_loop_t& loop, 
                    uv_udp_t& udphandle, 
                    const IpPort& listenAddr) {
    int retval = uv_udp_init(&loop, &udphandle);
    if (retval != 0) {
        LOGE << "uv_udp_init: " << uv_strerror(retval);
        return false;
    }
    Endpoint ep(AF_INET, listenAddr.ip, listenAddr.port);
    retval = uv_udp_bind(&udphandle, ep.addr(), UV_UDP_REUSEADDR);
    if (retval != 0) {
        LOGE << "uv_udp_bind: " << uv_strerror(retval);
        return false;
    }
    retval = uv_udp_recv_start(&udphandle, handleAlloc, handleRecv);
    if (retval != 0) {
        LOGE << "uv_udp_recv_start: " << uv_strerror(retval);
        return false;
    }
    LOGI << "udp service listen on " << ep.ip() << ":" << ep.port();
    return true;
}

int main(int argc, char* argv[]) {
    std::string listenAddrStr;
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
        }
    }

    if (listenAddrStr.empty()) {
        print_opt(kOptions);
        return 1;
    }

    IpPort listenAddr;
    if (!util::parseIpPort(listenAddrStr, listenAddr)) {
        LOGE << "invalid argument " << listenAddrStr;
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
        if (!udpInit(mainloop, udphandle, listenAddr)) {
            return;
        }
    });

    t.join();
    uv_loop_close(&mainloop);

    return 0;
}