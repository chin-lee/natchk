#include "udpsvc.h"
#include "endpoint.h"
#include "log.h"
#include "async.h"
#include <vector>
#include <algorithm>

typedef UdpService::ShutdownCallback ShutdownCallback;
typedef UdpService::IMessageHandler IMessageHandler;

struct SendReq {
    uv_udp_send_t handle;
    Endpoint peer;
    int size;
    char data[1];

    static SendReq* create(const Endpoint& peer, const char* data, int size) {
        SendReq* req = (SendReq*)malloc(sizeof(SendReq) + size);
        new(&req->peer) Endpoint(peer);
        req->size = size;
        memcpy(req->data, data, size);
        return req;
    }

    void destroy() {
        free(this);
    }

    uv_buf_t buf() {
        return uv_buf_t{data, size_t(size)};
    }
};

// -----------------------------------------------------------------------------
// Section: UdpServiceImpl
// -----------------------------------------------------------------------------
class UdpServiceImpl {
    UdpService& m_udpSvc;
    uv_loop_t& m_loop;
    uv_udp_t m_udpHandle;
    Endpoint m_listenAddr;
    AsyncHandler m_asyncHandler;
    ShutdownCallback m_shutdownCallback;
    std::vector<IMessageHandler*> m_msgHandlers;

private:
    static void allocRecvBuf(uv_handle_t* handle, 
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
        UdpServiceImpl* udpSvc = 
                CONTAINER_OF(handle, UdpServiceImpl, m_udpHandle);
        if (0 == nread && NULL == addr) {
            free(buf->base);
            return;
        }
        Endpoint peer(addr);
        if ( (flags & UV_UDP_PARTIAL) != 0 ) {
            LOGE << "partial data received from " << peer.ip() << ":" 
                 << peer.port();
        } else if (nread > 0) {
            udpSvc->handleMessage(peer, buf->base, buf->len);
        }
        free(buf->base);
    }

    static void handleSend(uv_udp_send_t* req, int status) {
        SendReq* sendReq = CONTAINER_OF(req, SendReq, handle);
        if (status) {
            LOGE << "error sending message to " << sendReq->peer.ip() 
                 << ":" << sendReq->peer.port() << ": " << uv_strerror(status);
        }
        sendReq->destroy();
    }

    static void handleClose(uv_handle_t* handle) {
        UdpServiceImpl* udpSvc = 
            CONTAINER_OF(handle, UdpServiceImpl, m_udpHandle);
        ShutdownCallback cb(std::move(udpSvc->m_shutdownCallback));
        delete udpSvc;
        cb();
    }

public:
    UdpServiceImpl(UdpService& udpSvc, uv_loop_t& loop, 
                   const Endpoint& listenAddr)
        : m_udpSvc(udpSvc), m_loop(loop), m_listenAddr(listenAddr)
        , m_asyncHandler(loop) {
        m_msgHandlers.reserve(128);
        m_asyncHandler.post([this]() {
            initUdpHandle();
        });
    }

    bool start() {
        return m_asyncHandler.post([this]() {
            int retval = uv_udp_recv_start(&m_udpHandle, 
                    allocRecvBuf, handleRecv);
            if (retval != 0) {
                LOGE << "uv_udp_recv_start: " << uv_strerror(retval);
            } else {
                LOGI << "udp service listening on " << m_listenAddr.ip() 
                     << ":" << m_listenAddr.port();
            }
        });
    }

    bool send(const Endpoint& peer, const char* data, int size) {
        SendReq* req = SendReq::create(peer, data, size);
        return m_asyncHandler.post([this, req]() {
            uv_buf_t bufs[1] = { req->buf() };
            int retval = uv_udp_send(&req->handle, &m_udpHandle, bufs, 1, 
                                     req->peer, handleSend);
            if (retval != 0) {
                LOGE << "uv_udp_send: " << uv_strerror(retval);
            }
        });
    }

    bool shutdown(std::function<void()>&& callback) {
        return m_asyncHandler.post([this, cb(std::move(callback))]() {
            if (uv_is_active((uv_handle_t*)&m_udpHandle)) {
                int retval = uv_udp_recv_stop(&m_udpHandle);
                if (retval != 0) {
                    LOGW << "uv_udp_recv_stop: " << uv_strerror(retval);
                }
            }
            uv_close((uv_handle_t*)&m_udpHandle, handleClose);
            m_shutdownCallback = std::move(cb);
        });
    }

    bool shutdown() {
        uv_barrier_t barrier;
        uv_barrier_init(&barrier, 2);
        bool retval = shutdown([&barrier]() {
            uv_barrier_wait(&barrier);
        });
        if (!retval) {
            uv_barrier_destroy(&barrier);
            return false;
        }
        uv_barrier_wait(&barrier);
        uv_barrier_destroy(&barrier);
        return true;
    }

    void addMessageHandler(IMessageHandler* handler) {
        m_asyncHandler.post([this, handler]() {
            if (std::find(m_msgHandlers.begin(), m_msgHandlers.end(), 
                          handler) == m_msgHandlers.end()) {
                m_msgHandlers.push_back(handler);
            }
        });
    }

    void removeMessageHandler(IMessageHandler* handler) {
        m_asyncHandler.post([this, handler]() {
            auto it = std::find(m_msgHandlers.begin(), 
                                m_msgHandlers.end(), handler);
            if (it != m_msgHandlers.end()) {
                m_msgHandlers.erase(it);
            }
        });
    }

private:
    bool initUdpHandle() {
        int retval = uv_udp_init(&m_loop, &m_udpHandle);
        if (retval != 0) {
            LOGE << "uv_udp_init: " << uv_strerror(retval);
            return false;
        }
        retval = uv_udp_bind(&m_udpHandle, m_listenAddr, UV_UDP_REUSEADDR);
        if (retval != 0) {
            LOGE << "uv_udp_bind: " << uv_strerror(retval);
            return false;
        }
        return true;
    }

    void handleMessage(const Endpoint& addr, const char* data, int size) {
        for (IMessageHandler* handler : m_msgHandlers) {
            handler->handleMessage(m_udpSvc, addr, data, size);
        }
    }
};

// -----------------------------------------------------------------------------
// Section: UdpService
// -----------------------------------------------------------------------------
UdpService::UdpService(uv_loop_t& loop, const Endpoint& listenAddr) 
    : m_pImpl(new UdpServiceImpl(*this, loop, listenAddr))
    , m_impl(*m_pImpl) {
}

UdpService::~UdpService() {
    if (NULL != m_pImpl) {
        m_impl.shutdown([]() {
            // |m_pImpl| will be destroyed when uv_udp_t is closed
        });
    }
}

bool UdpService::start() {
    return m_impl.start();
}

bool UdpService::shutdown(ShutdownCallback&& callback) {
    bool retval = m_impl.shutdown(std::move(callback));
    if (retval) {
        m_pImpl = NULL;
    }
    return retval;
}

bool UdpService::shutdown() {
    bool retval = m_impl.shutdown();
    if (retval) {
        m_pImpl = NULL;
    }
    return retval;
}

bool UdpService::send(const Endpoint& peer, const char* data, int size) {
    return m_impl.send(peer, data, size);
}

void UdpService::addMessageHandler(IMessageHandler* handler) {
    m_impl.addMessageHandler(handler);
}

void UdpService::removeMessageHandler(IMessageHandler* handler) {
    m_impl.removeMessageHandler(handler);
}