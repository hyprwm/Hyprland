#pragma once
#include <cstdint>
#include <string>
#include <format>
#include <iostream>
#include <fstream>
#include <chrono>
#include <mutex>

#define LOGMESSAGESIZE   1024
#define ROLLING_LOG_SIZE 4096

enum eLogLevel : int8_t {
    NONE  = -1,
    LOG   = 0,
    WARN  = 1,
    ERR   = 2,
    CRIT  = 3,
    INFO  = 4,
    TRACE = 5
};

// NOLINTNEXTLINE(readability-identifier-naming)
namespace NDebug {
    inline std::string     logFile;
    inline std::ofstream   logOfs;
    inline int64_t* const* g_disableLogs = nullptr;
    inline int64_t* const* g_disableTime = nullptr;
    inline bool            disableStdout = false;
    inline bool            trace         = false;
    inline bool            shuttingDown  = false;
    inline int64_t* const* g_coloredLogs = nullptr;

    inline std::string     rollingLog = ""; // rolling log contains the ROLLING_LOG_SIZE tail of the log
    inline std::mutex      logMutex;

    void                   init(const std::string& IS);
    void                   close();

    //
    void log(eLogLevel level, std::string str);

    template <typename... Args>
    //NOLINTNEXTLINE
    void log(eLogLevel level, std::format_string<Args...> fmt, Args&&... args) {
        std::lock_guard<std::mutex> guard(logMutex);

        if (level == TRACE && !trace)
            return;

        if (shuttingDown)
            return;

        std::string logMsg = "";

        // print date and time to the ofs
        if (g_disableTime && !**g_disableTime) {
#ifndef _LIBCPP_VERSION
            static auto current_zone = std::chrono::current_zone();
            const auto  ZT           = std::chrono::zoned_time{current_zone, std::chrono::system_clock::now()};
            const auto  HMS          = std::chrono::hh_mm_ss{ZT.get_local_time() - std::chrono::floor<std::chrono::days>(ZT.get_local_time())};
#else
            // TODO: current clang 17 does not support `zoned_time`, remove this once clang 19 is ready
            const auto HMS = std::chrono::hh_mm_ss{std::chrono::system_clock::now() - std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now())};
#endif
            logMsg += std::format("[{}] ", HMS);
        }

        // no need for try {} catch {} because std::format_string<Args...> ensures that vformat never throw std::format_error
        // because
        // 1. any faulty format specifier that sucks will cause a compilation error.
        // 2. and `std::bad_alloc` is catastrophic, (Almost any operation in stdlib could throw this.)
        // 3. this is actually what std::format in stdlib does
        logMsg += std::vformat(fmt.get(), std::make_format_args(args...));

        log(level, logMsg);
    }
};
