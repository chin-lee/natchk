#pragma once

#include <string>
#include <vector>
#include <sys/time.h>
#include <stdlib.h>
#include <stdint.h>

#define DEFAULT_COPY_MOVE_AND_ASSIGN(TypeName)                                \
    TypeName(const TypeName&) = default;                                      \
    TypeName(TypeName&&) = default;                                           \
    TypeName& operator=(const TypeName&) = default;                           \
    TypeName& operator=(TypeName&&) = default

#define DISALLOW_COPY_MOVE_AND_ASSIGN(TypeName)                               \
private:                                                                      \
    TypeName(const TypeName&);                                                \
    TypeName& operator=(const TypeName&);                                     \
    TypeName(TypeName&&);                                                     \
    TypeName& operator=(const TypeName&&)

#define CONTAINER_OF(ptr, type, field)                                        \
    ((type*)((char*)(ptr) - ((char*)&((type*)0)->field)))

#define ARRAY_SIZE(a) (sizeof((a)) / sizeof((a)[0]))

struct IpPort {
    std::string ip;
    uint16_t port;
};

namespace util {

inline uint64_t currentMilliSecs() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

inline bool parseIpPort(const std::string& src, 
                        IpPort& addr) {
    int pos = src.find_first_of(":");
    if (pos > 0) {
        addr.ip = src.substr(0, pos);
        addr.port = uint16_t( atoi(src.substr(pos + 1).c_str()) );
    } else if (pos == 0) {
        addr.ip = "0.0.0.0";
        addr.port = uint16_t( atoi(src.substr(pos + 1).c_str()) );
    } else {
        return false;
    }
    return true;
}

inline bool parseIpPortList(const std::string& src, 
                            std::vector<IpPort>& addrs) {
    int startPos = 0;
    int pos = 0;
    while (startPos < src.size()) {
        pos = src.find_first_of(",", startPos);
        if (pos < 0) {
            break;
        }
        std::string token = src.substr(startPos, pos - startPos);
        IpPort addrEntry;
        if (!parseIpPort(token, addrEntry)) {
            return false;
        }
        addrs.emplace_back(std::move(addrEntry));
        startPos = pos + 1;
    }
    std::string token = src.substr(startPos);
    if (!token.empty()) {
        IpPort addrEntry;
        if (!parseIpPort(token, addrEntry)) {
            return false;
        }
        addrs.emplace_back(std::move(addrEntry));
    }
    return true;
}

} // namespace util
