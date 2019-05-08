#include "endpoint.h"
#include <string.h>

Endpoint::Endpoint() {
  memset(&m_sockAddr, 0, sizeof(m_sockAddr));
}

Endpoint::Endpoint(int af, const std::string& ip, uint16_t port) {
    init(af, ip, port);
}

Endpoint::Endpoint(const struct sockaddr* addr) {
    init(addr);
}

bool Endpoint::init(int af, const std::string& ip, uint16_t port) {
    m_sockAddr.s.sa_family = af;
    if (AF_INET == af) {
        uv_ip4_addr(ip.c_str(), port, &m_sockAddr.v4);
    } else if (AF_INET6 == af) {
        uv_ip6_addr(ip.c_str(), port, &m_sockAddr.v6);
    }
    return false;
}

bool Endpoint::init(const struct sockaddr* addr) {
    m_sockAddr.s.sa_family = addr->sa_family;
    if (AF_INET == addr->sa_family) {
        memcpy(&m_sockAddr.v4, addr, sizeof(struct sockaddr_in));
    } else if (AF_INET6 == addr->sa_family) {
        memcpy(&m_sockAddr.v6, addr, sizeof(struct sockaddr_in6));
    }
    return false;
}

const std::string& Endpoint::ip() const {
    if (!m_ip.empty()) {
        return m_ip;
    }
    if (AF_INET == m_sockAddr.s.sa_family) {
        char buf[INET_ADDRSTRLEN];
        memset(buf, 0, sizeof(buf));
        inet_ntop(AF_INET, (void*)&m_sockAddr.v4.sin_addr, buf, sizeof(buf));
        m_ip = buf;
    } else if (AF_INET6 == m_sockAddr.s.sa_family) {
        char buf[INET6_ADDRSTRLEN];
        memset(buf, 0, sizeof(buf));
        inet_ntop(AF_INET6, (void*)&m_sockAddr.v6.sin6_addr, buf, sizeof(buf));
        m_ip = buf;
    }
    return m_ip;
}

uint16_t Endpoint::port() const {
    if (AF_INET == m_sockAddr.s.sa_family) {
        return ntohs(m_sockAddr.v4.sin_port);
    } else if (AF_INET6 == m_sockAddr.s.sa_family) {
        return ntohs(m_sockAddr.v6.sin6_port);
    } else {
        return 0;
    }
}

const struct sockaddr* Endpoint::sockaddr() const {
    return &m_sockAddr.s;
}

Endpoint::operator ConstSockAddrPtr() {
    return &m_sockAddr.s;
}

Endpoint::operator ConstSockAddrPtr() const {
    return &m_sockAddr.s;
}

const struct sockaddr_in* Endpoint::v4() const {
    return ( (AF_INET == m_sockAddr.s.sa_family) ? &m_sockAddr.v4 : NULL);
}

const struct sockaddr_in6* Endpoint::v6() const {
    return ( (AF_INET6 == m_sockAddr.s.sa_family) ? &m_sockAddr.v6 : NULL );
}

int Endpoint::serializeToArray(char* buf, int size) const {
    int sizeReq = 0;
    if (AF_INET == m_sockAddr.s.sa_family) {
        sizeReq = sizeof(struct sockaddr_in);
    } else if (AF_INET6 == m_sockAddr.s.sa_family) {
        sizeReq = sizeof(struct sockaddr_in6);
    } else {
        return 0;
    }
    if (size < sizeReq) {
        return 0;
    }
    memcpy(buf, &m_sockAddr, sizeReq);
    return sizeReq;
}

bool Endpoint::parseFromArray(const char* buf, int size) {
    const struct sockaddr* sa = (const struct sockaddr*)buf;
    int sizeReq = 0;
    if (AF_INET == sa->sa_family) {
        sizeReq = sizeof(struct sockaddr_in);
    } else if (AF_INET6 == sa->sa_family) {
        sizeReq = sizeof(struct sockaddr_in6);
    } else {
        return false;
    }
    if (size < sizeReq) {
        return false;
    }
    return init(sa);
}