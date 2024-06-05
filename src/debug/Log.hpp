#pragma once
#include <string>
#include <format>
#include <iostream>
#include <fstream>
#include <chrono>
#include "../includes.hpp"
#include "../helpers/MiscFunctions.hpp"

#define LOGMESSAGESIZE   1024
#define ROLLING_LOG_SIZE 4096

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
    inline std::string     logFile;
    inline int64_t* const* disableLogs   = nullptr;
    inline int64_t* const* disableTime   = nullptr;
    inline bool            disableStdout = false;
    inline bool            trace         = false;
    inline bool            shuttingDown  = false;
    inline int64_t* const* coloredLogs   = nullptr;

    inline std::string     rollingLog = ""; // rolling log contains the ROLLING_LOG_SIZE tail of the log

    void                   init(const std::string& IS);

    //
    void log(LogLevel level, std::string str);

    template <typename... Args>
    void log(LogLevel level, std::format_string<Args...> fmt, Args&&... args) {
        if (level == TRACE && !trace)
            return;

        if (shuttingDown)
            return;

        std::string logMsg = "";

        // print date and time to the ofs
        if (disableTime && !**disableTime) {
#ifndef _LIBCPP_VERSION
            const auto zt  = std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::system_clock::now()};
            const auto hms = std::chrono::hh_mm_ss{zt.get_local_time() - std::chrono::floor<std::chrono::days>(zt.get_local_time())};
#else
            // TODO: current clang 17 does not support `zoned_time`, remove this once clang 19 is ready
            const auto hms = std::chrono::hh_mm_ss{std::chrono::system_clock::now() - std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now())};
#endif
            logMsg += std::format("[{}] ", hms);
        }

        // no need for try {} catch {} because std::format_string<Args...> ensures that vformat never throw std::format_error
        // because
        // 1. any faulty format specifier that sucks will cause a compilation error.
        // 2. and `std::bad_alloc` is catastrophic, (Almost any operation in stdlib could throw this.)
        // 3. this is actually what std::format in stdlib does
        logMsg += std::vformat(fmt.get(), std::make_format_args(args...));

        log(level, logMsg);
    }

    void wlrLog(wlr_log_importance level, const char* fmt, va_list args);
};
