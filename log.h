#pragma once

#include <sstream>
#include <iostream>
#include <chrono>
#include <string.h>

enum LogSeverity {
    TRACE, 
    DEBUG, 
    INFO, 
    WARN, 
    ERROR
};

class Logger {
    std::stringstream m_ss;

public:
    Logger(int level, const char* file, int line, const char* func) {
        auto tpnow = std::chrono::system_clock::now();
        auto nanosecs = tpnow.time_since_epoch();
        auto secs = std::chrono::duration_cast<std::chrono::seconds>(nanosecs);
        auto microsecs = std::chrono::duration_cast<std::chrono::microseconds>(
                nanosecs - secs).count();
        auto ts = std::chrono::system_clock::to_time_t(tpnow);
        auto tm = *std::localtime(&ts);
        char buf[64];
        memset(buf, 0, sizeof(buf));
        int len = snprintf(buf, sizeof(buf), 
                           "%04d/%02d/%02d %02d:%02d:%02d.%06d",
                           tm.tm_year + 1900, tm.tm_mon, tm.tm_mday,
                           tm.tm_hour, tm.tm_min, tm.tm_sec,
                           static_cast<int>(microsecs));
        m_ss.write(buf, len);
        m_ss << "|";

        switch (level) {
        case TRACE:
            m_ss << "TRACE";
            break;
        case DEBUG:
            m_ss << "DEBUG";
            break;
        case INFO:
            m_ss << "INFO ";
            break;
        case WARN:
            m_ss << "WARN ";
            break;
        case ERROR:
            m_ss << "ERROR";
            break;
        }
        m_ss << "|";

        const char* slashPos = strrchr(file, '/');
        m_ss << (slashPos ? slashPos + 1 : file);
        m_ss << ":" << line << "|" << func << "|";
    }

    ~Logger() {
        std::cout << m_ss.str() << std::endl;
    }

    std::stringstream& stream() {
        return m_ss;
    }
};

#define LOG(LEVEL)                                                            \
    Logger(LEVEL, __FILE__, __LINE__, __FUNCTION__).stream()

#define LOGT LOG(TRACE)
#define LOGD LOG(DEBUG)
#define LOGI LOG(INFO)
#define LOGW LOG(WARN)
#define LOGE LOG(ERROR)
