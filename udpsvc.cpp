#include "udpsvc.h"
#include "endpoint.h"
#include "log.h"

// -----------------------------------------------------------------------------
// Section: UdpServiceImpl
// -----------------------------------------------------------------------------
class UdpServiceImpl {
    uv_loop_t& m_loop;
    uv_udp_t m_udpHandle;

    static void handleAlloc(uv_handle_t* handle, 
                            size_t suggested_size, 
                            uv_buf_t* buf) {

    }

    static void handleRecv(uv_udp_t* handle, ssize_t nread,
                           const uv_buf_t* buf, 
                           const struct sockaddr* addr,
                           unsigned flags) {
    }

public:
    explicit UdpServiceImpl(uv_loop_t& loop) : m_loop(loop) {
    }

    bool start(const Endpoint& listenAddr) {
        int retval = uv_udp_init(&m_loop, &m_udpHandle);
        if (retval != 0) {
            LOGE << "uv_udp_init: " << uv_strerror(retval);
            return false;
        }
        retval = uv_udp_bind(&m_udpHandle, listenAddr, UV_UDP_REUSEADDR);
        if (retval != 0) {
            LOGE << "uv_udp_bind: " << uv_strerror(retval);
            return false;
        }
        retval = uv_udp_recv_start(&m_udpHandle, handleAlloc, handleRecv);
        if (retval != 0) {
            LOGE << "uv_udp_recv_start: " << uv_strerror(retval);
            return false;
        }
    }
};

// -----------------------------------------------------------------------------
// Section: UdpService
// -----------------------------------------------------------------------------
