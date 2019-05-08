#pragma once

#include "uv.h"
#include <functional>

class UdpServiceImpl;
class Endpoint;

class UdpService {
public:
    UdpService(uv_loop_t& loop, const Endpoint& listenAddr);
    ~UdpService();

    bool start();

    typedef std::function<void()> ShutdownCallback;

    bool shutdown(ShutdownCallback&& callback);
    bool shutdown();

    bool send(const Endpoint& peer, const char* data, int size);

    struct IMessageHandler {
        virtual ~IMessageHandler() { }
        virtual void handleMessage(UdpService& udpSvc, const Endpoint& peer, 
                const char* data, int size) = 0;
    };
    void addMessageHandler(IMessageHandler* handler);
    void removeMessageHandler(IMessageHandler* handler);

private:
    UdpServiceImpl* m_pImpl;
    UdpServiceImpl& m_impl;
};