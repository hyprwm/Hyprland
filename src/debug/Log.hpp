#pragma once
#include <string>

#define LOGMESSAGESIZE 1024

enum LogLevel {
    NONE = -1,
    LOG = 0,
    WARN,
    ERR,
    CRIT,
    INFO
};

namespace Debug {
    void init(std::string IS);
    void log(LogLevel level, const char* fmt, ...);

    inline std::string logFile;
    inline int64_t* disableLogs = nullptr;
};