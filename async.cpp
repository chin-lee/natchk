#include "async.h"
#include "util.h"

#include <vector>
#include <mutex>

typedef AsyncHandler::HandlerType HandlerType;
typedef AsyncHandler::ShutdownHandlerType ShutdownHandlerType;
// -----------------------------------------------------------------------------
// Section: AsyncHandlerImpl
// -----------------------------------------------------------------------------
class AsyncHandlerImpl {
    static const int kInitialHandlerListSize = 128;

    uv_loop_t& m_loopHandle;
    uv_async_t m_asyncHandle;

    std::vector<HandlerType> m_handlerList;
    std::mutex m_handlerMutex;

    ShutdownHandlerType m_shutdownHandler;

public:
    static void handlerPost(uv_async_t* handle) {
        AsyncHandlerImpl* h = 
            CONTAINER_OF(handle, AsyncHandlerImpl, m_asyncHandle);
        h->invokeHandler();
    }

    static void handleClose(uv_handle_t* handle) {
        AsyncHandlerImpl* h = 
            CONTAINER_OF(handle, AsyncHandlerImpl, m_asyncHandle);
        ShutdownHandlerType handler(std::move(h->m_shutdownHandler));
        delete h;
        handler();
    }

public:
    explicit AsyncHandlerImpl(uv_loop_t& loop) : m_loopHandle(loop) {
        uv_async_init(&loop, &m_asyncHandle, handlerPost);
        m_handlerList.reserve(kInitialHandlerListSize);
    }

    bool post(HandlerType&& handler) {
        {
            std::unique_lock<std::mutex> l(m_handlerMutex);
            m_handlerList.emplace_back(std::move(handler));
        }
        int retval = uv_async_send(&m_asyncHandle);
        if (retval != 0) {
            // TODO: log error
            return false;
        }
        return true;
    }

    bool shutdown(ShutdownHandlerType&& handler) {
        return post([this, hlr(std::move(handler))]() {
            if (uv_is_active((uv_handle_t*)&m_asyncHandle)) {
                uv_close((uv_handle_t*)&m_asyncHandle, handleClose);
                m_shutdownHandler = std::move(hlr);
            }
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

private:
    void invokeHandler() {
        std::vector<HandlerType> handlers;
        {
            std::unique_lock<std::mutex> l(m_handlerMutex);
            m_handlerList.swap(handlers);
            m_handlerList.reserve(kInitialHandlerListSize);
        }
        for (auto& h : handlers) {
            h();
        }
    }
};

// -----------------------------------------------------------------------------
// Section: AsyncHandlerImpl
// -----------------------------------------------------------------------------
AsyncHandler::AsyncHandler(uv_loop_t& loop) 
    : m_pImpl(new AsyncHandlerImpl(loop)), m_impl(*m_pImpl) {
}

AsyncHandler::~AsyncHandler() {
    if (NULL != m_pImpl) {
        m_impl.shutdown();
        delete m_pImpl;
    }
}

bool AsyncHandler::post(HandlerType&& handler) {
    return m_impl.post(std::move(handler));
}

bool AsyncHandler::shutdown(ShutdownHandlerType&& handler) {
    return m_impl.shutdown(std::move(handler));
}

bool AsyncHandler::shutdown() {
    return m_impl.shutdown();
}