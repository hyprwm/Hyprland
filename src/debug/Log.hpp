#pragma once
#include <string>

#define LOGMESSAGESIZE 1024

enum LogLevel {
    NONE = -1,
    LOG = 0,
    WARN,
    ERR,
    CRIT
};

namespace Debug {
    void init();
    void log(LogLevel level, const char* fmt, ...);

    inline std::string logFile;
};