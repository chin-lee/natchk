#include "endpoint.h"
#include <string.h>

Endpoint::Endpoint(int family, const std::string& ip, uint16_t port) 
    : m_ip(ip) {
    m_addr.sa.sa_family = family;
    if ( (AF_INET == family) || (AF_INET6 == family) ) {
        
    }
    if (AF_INET == family) {
        inet_pton(family, ip.c_str(), &m_addr.v4.sin_addr);
        m_addr.v4.sin_port = htons(port);
    } else if (AF_INET6 == family) {
        inet_pton(family, ip.c_str(), &m_addr.v6.sin6_addr);
        m_addr.v6.sin6_port = htons(port);
    }
}

Endpoint::Endpoint(const struct sockaddr* addr) {
   parseFromSockaddr(addr);
}

const std::string& Endpoint::ip() const {
    return m_ip;
}

uint16_t Endpoint::port() const {
    if (AF_INET == m_addr.sa.sa_family) {
        return ntohs(m_addr.v4.sin_port);
    } else if (AF_INET6 == m_addr.sa.sa_family) {
        return ntohs(m_addr.v6.sin6_port);
    } else {
        return 0;
    }
}

const struct sockaddr* Endpoint::addr() const {
    return &m_addr.sa;
}

const struct sockaddr_in* Endpoint::addr4() const {
    return &m_addr.v4;
}

const struct sockaddr_in6* Endpoint::addr6() const {
    return &m_addr.v6;
}

bool Endpoint::parseFromSockaddr(const struct sockaddr* addr) {
    char buf[INET6_ADDRSTRLEN];
    bzero(buf, sizeof(buf));
    if (AF_INET == addr->sa_family) {
        memcpy((void*)&m_addr, addr, sizeof(struct sockaddr_in));
        inet_ntop(addr->sa_family, &((struct sockaddr_in*)(addr))->sin_addr, 
                    buf, INET_ADDRSTRLEN);
    } else if (AF_INET6 == addr->sa_family) {
        memcpy((void*)&m_addr, addr, sizeof(struct sockaddr_in6));
        inet_ntop(addr->sa_family, &((struct sockaddr_in6*)(addr))->sin6_addr, 
                    buf, INET6_ADDRSTRLEN);
    } else {
        return false;
    }
    m_ip = buf;
    return true;
}