#pragma once
#include <string>
#include <wlr/util/log.h>
#include <format>
#include <iostream>
#include <fstream>
#include <chrono>

#define LOGMESSAGESIZE 1024

enum LogLevel
{
    NONE = -1,
    LOG  = 0,
    WARN,
    ERR,
    CRIT,
    INFO,
    TRACE
};

namespace Debug {
    inline std::string logFile;
    inline int64_t*    disableLogs   = nullptr;
    inline int64_t*    disableTime   = nullptr;
    inline bool        disableStdout = false;
    inline bool        trace         = false;

    void               init(const std::string& IS);
    template <typename... Args>
    void log(LogLevel level, const std::string& fmt, Args&&... args) {
        if (disableLogs && *disableLogs)
            return;

        if (level == TRACE && !trace)
            return;

        std::string logMsg = "";

        switch (level) {
            case LOG: logMsg += "[LOG] "; break;
            case WARN: logMsg += "[WARN] "; break;
            case ERR: logMsg += "[ERR] "; break;
            case CRIT: logMsg += "[CRITICAL] "; break;
            case INFO: logMsg += "[INFO] "; break;
            case TRACE: logMsg += "[TRACE] "; break;
            default: break;
        }

        // log to a file
        std::ofstream ofs;
        ofs.open(logFile, std::ios::out | std::ios::app);

        // print date and time to the ofs
        if (disableTime && !*disableTime)
            logMsg += std::format("[{:%T}] ", std::chrono::hh_mm_ss{std::chrono::system_clock::now() - std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now())});

        logMsg += std::vformat(fmt, std::make_format_args(args...));

        ofs << logMsg << "\n";

        ofs.close();

        // log it to the stdout too.
        if (!disableStdout)
            std::cout << logMsg << "\n";
    }

    void wlrLog(wlr_log_importance level, const char* fmt, va_list args);
};