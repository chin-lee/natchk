#pragma once

#include "util.h"
#include "uv.h"
#include <string>
#include <string.h>

class Endpoint {
public:
    typedef const struct sockaddr* ConstSockAddrPtr;

    Endpoint();
    Endpoint(int af, const std::string& ip, uint16_t port);
    explicit Endpoint(const struct sockaddr* addr);

    DEFAULT_COPY_MOVE_AND_ASSIGN(Endpoint);

    bool init(int af, const std::string& ip, uint16_t port);
    bool init(const struct sockaddr* addr);

    const std::string& ip() const;
    uint16_t port() const;

    ConstSockAddrPtr sockaddr() const;
    operator ConstSockAddrPtr();
    operator ConstSockAddrPtr() const;

    const struct sockaddr_in* v4() const;
    const struct sockaddr_in6* v6() const;

    bool serializeToArray(char* buf, int size) const;
    bool parseFromArray(const char* buf, int size);

    friend bool operator<(const Endpoint& l, const Endpoint& r);
    friend bool operator==(const Endpoint& l, const Endpoint& r);

private:
    union {
        struct sockaddr_in v4;
        struct sockaddr_in6 v6;
        struct sockaddr s;
    } m_sockAddr;
    mutable std::string m_ip;
};

inline bool operator<(const Endpoint& l, const Endpoint& r) {
    int retval = l.m_ip.compare(r.m_ip);
    if (0 == retval) {
        return (l.port() < r.port());
    } else {
        return (retval < 0);
    }
}

inline bool operator==(const Endpoint& l, const Endpoint& r) {
    if (l.m_sockAddr.s.sa_family != r.m_sockAddr.s.sa_family) {
        return false;
    } else if (l.m_sockAddr.s.sa_family == AF_INET) {
        return (0 == memcmp((const void*)&l.m_sockAddr.v4,
                            (const void*)&r.m_sockAddr.v4,
                            sizeof(struct sockaddr_in) ) );
    } else if (l.m_sockAddr.s.sa_family == AF_INET6) {
        return (0 == memcmp((const void*)&l.m_sockAddr.v6,
                            (const void*)&r.m_sockAddr.v6,
                            sizeof(struct sockaddr_in6) ) );
    }
    return false;
}