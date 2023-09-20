#pragma once
#include <string>
#include <wlr/util/log.h>
#include <format>
#include <iostream>
#include <fstream>
#include <chrono>
#include "../helpers/MiscFunctions.hpp"

#define LOGMESSAGESIZE 1024

enum LogLevel {
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
    void log(LogLevel level, std::format_string<Args...> fmt, Args&&... args) {
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
        if (disableTime && !*disableTime) {
#ifndef _LIBCPP_VERSION
            logMsg += std::format("[{:%T}] ", std::chrono::hh_mm_ss{std::chrono::system_clock::now() - std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now())});
#else
            auto c = std::chrono::hh_mm_ss{std::chrono::system_clock::now() - std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now())};
            logMsg += std::format("{:%H}:{:%M}:{:%S}", c.hours(), c.minutes(), c.subseconds());

#endif
        }

        // no need for try {} catch {} because std::format_string<Args...> ensures that vformat never throw std::format_error
        // because
        // 1. any faulty format specifier that sucks will cause a compilation error.
        // 2. and `std::bad_alloc` is catastrophic, (Almost any operation in stdlib could throw this.)
        // 3. this is actually what std::format in stdlib does
        logMsg += std::vformat(fmt.get(), std::make_format_args(args...));

        ofs << logMsg << "\n";

        ofs.close();

        // log it to the stdout too.
        if (!disableStdout)
            std::cout << logMsg << "\n";
    }

    void wlrLog(wlr_log_importance level, const char* fmt, va_list args);
};