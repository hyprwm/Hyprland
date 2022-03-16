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
    void log(LogLevel level, const char* fmt, ...);
};