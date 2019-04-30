#pragma once

#include <uv.h>
#include <functional>

class AsyncHandlerImpl;

class AsyncHandler {
public:
    typedef std::function<void()> HandlerType;
    typedef std::function<void()> ShutdownHandlerType;

    explicit AsyncHandler(uv_loop_t& loop);
    ~AsyncHandler();

    // |handler| will be called on the thread who runs the loop
    bool post(HandlerType&& handler);

    // |handler| will be called on the thread who runs the loop
    bool shutdown(ShutdownHandlerType&& handler);

    // This function will block until the inernal uv_async_t is shutdown
    bool shutdown();

private:
    AsyncHandlerImpl* m_pImpl;
    AsyncHandlerImpl& m_impl;
};