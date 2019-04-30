#pragma once

#include <sstream>
#include <iostream>
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
        switch (level) {
        case TRACE:
            m_ss << "TRAC";
            break;
        case DEBUG:
            m_ss << "DEBG";
            break;
        case INFO:
            m_ss << "INFO";
            break;
        case WARN:
            m_ss << "WARN";
            break;
        case ERROR:
            m_ss << "ERRO";
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
