#pragma once

#include "util.h"

#include <string>
#include <arpa/inet.h>
#include <string.h>

class Endpoint {
public:
    friend bool operator<(const Endpoint& l, const Endpoint& r);
    friend bool operator==(const Endpoint& l, const Endpoint& r);

    Endpoint(int family, const std::string& ip, uint16_t port);
    explicit Endpoint(const struct sockaddr* addr);

    DEFAULT_COPY_MOVE_AND_ASSIGN(Endpoint);

    const std::string& ip() const;
    uint16_t port() const;
    const struct sockaddr* addr() const;
    const struct sockaddr_in* addr4() const;
    const struct sockaddr_in6* addr6() const;

    bool serializeToArray(char* buf, int size) const;
    bool parseFromArray(const char* buf, int size);

private:
    bool parseFromSockaddr(const struct sockaddr* addr);

private:
    std::string m_ip;
    union {
        struct sockaddr_in v4;
        struct sockaddr_in6 v6;
        struct sockaddr sa;
    } m_addr;
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
    if (l.m_addr.sa.sa_family != r.m_addr.sa.sa_family) {
        return false;
    } else if (l.m_addr.sa.sa_family == AF_INET) {
        return (0 == memcmp((const void*)&l.m_addr.v4, 
                            (const void*)&r.m_addr.v4, 
                            sizeof(struct sockaddr_in) ) );
    } else if (l.m_addr.sa.sa_family == AF_INET6) {
        return (0 == memcmp((const void*)&l.m_addr.v6, 
                            (const void*)&r.m_addr.v6, 
                            sizeof(struct sockaddr_in6) ) );
    }
    return false;
}