#pragma once

#include "uv.h"
#include <functional>

class UdpServiceImpl;
class Endpoint;

class UdpService {
public:
    explicit UdpService(uv_loop_t& loop);

    bool start(const Endpoint& listenAddr);
    bool shutdown(std::function<void()>&& callback);
    bool shutdown();

    bool send(const Endpoint& peer, const char* data, int size);

    struct IMessageHandler {
        virtual ~IMessageHandler() { }
        virtual void handleMessage(const Endpoint& peer, 
                const char* data, int size) = 0;
    };
    void addMessageHandler(IMessageHandler* handler);
    void removeMessageHandler(IMessageHandler* handler);

private:
    UdpServiceImpl* m_pImpl;
    UdpServiceImpl& m_impl;
};