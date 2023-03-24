#pragma once
#include <string>
#include <wlr/util/log.h>

#define LOGMESSAGESIZE 1024

enum LogLevel
{
    NONE = -1,
    LOG  = 0,
    WARN,
    ERR,
    CRIT,
    INFO
};

namespace Debug {
    void               init(const std::string& IS);
    void               log(LogLevel level, const char* fmt, ...);
    void               wlrLog(wlr_log_importance level, const char* fmt, va_list args);

    inline std::string logFile;
    inline int64_t*    disableLogs   = nullptr;
    inline int64_t*    disableTime   = nullptr;
    inline bool        disableStdout = false;
};