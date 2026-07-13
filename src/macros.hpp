#pragma once

#include <cmath>
#include <csignal>
#include <print>
#include <string_view>
#include <utility>

#include "helpers/memory/Memory.hpp"
#include "debug/log/Logger.hpp"

#ifndef NDEBUG
#ifdef HYPRLAND_DEBUG
#define ISDEBUG true
#else
#define ISDEBUG false
#endif
#else
#define ISDEBUG false
#endif

#define SPECIAL_WORKSPACE_START (-99)

#define STRVAL_EMPTY "[[EMPTY]]"

#define WORKSPACE_INVALID     -1L
#define WORKSPACE_NOT_CHANGED -101

#define MONITOR_INVALID  -1L
#define MONITOR_FALLBACK -2L

#define MIN_WINDOW_SIZE 20.0

// max value 32 because killed is a int uniform
#define POINTER_PRESSED_HISTORY_LENGTH 32

#define VECINRECT(vec, x1, y1, x2, y2)    ((vec).x >= (x1) && (vec).x < (x2) && (vec).y >= (y1) && (vec).y < (y2))
#define VECNOTINRECT(vec, x1, y1, x2, y2) ((vec).x < (x1) || (vec).x >= (x2) || (vec).y < (y1) || (vec).y >= (y2))

#define DELTALESSTHAN(a, b, delta) (abs((a) - (b)) < (delta))

#define STICKS(a, b) abs((a) - (b)) < 2

#define HYPRATOM(name) {name, 0}

template <typename... Args>
[[gnu::noinline]] [[gnu::cold]] void assertImpl(int line, std::string_view filename, std::format_string<Args...> reason, Args&&... args) {
    Log::logger->log(Log::CRIT, "\n==========================================================================================\nASSERTION FAILED! \n\n{}\n\nat: line {} in {}",
                     std::format(reason, std::forward<Args>(args)...), line, filename);
    std::print("Assertion failed! See the log in /tmp/hypr/hyprland.log for more info.");
    raise(SIGABRT);
}

#define RASSERT(expr, reason, ...)                                                                                                                                                 \
    if (!(expr)) [[unlikely]] {                                                                                                                                                    \
        constexpr auto FILENAME = std::string_view(__FILE__).substr(std::string_view(__FILE__).find_last_of('/') + 1);                                                             \
        assertImpl(__LINE__, FILENAME, reason, ##__VA_ARGS__);                                                                                                                     \
    }

#define ASSERT(expr) RASSERT(expr, "?")

// absolutely ridiculous formatter spec parsing
#define FORMAT_PARSE(specs__, type__)                                                                                                                                              \
    template <typename FormatContext>                                                                                                                                              \
    constexpr auto parse(FormatContext& ctx) {                                                                                                                                     \
        auto it = ctx.begin();                                                                                                                                                     \
        for (; it != ctx.end() && *it != '}'; it++) {                                                                                                                              \
            switch (*it) { specs__ default : throw std::format_error("invalid format specification"); }                                                                            \
        }                                                                                                                                                                          \
        return it;                                                                                                                                                                 \
    }

#define FORMAT_FLAG(spec__, flag__)                                                                                                                                                \
    case spec__: (flag__) = true; break;

#define FORMAT_NUMBER(buf__)                                                                                                                                                       \
    case '0':                                                                                                                                                                      \
    case '1':                                                                                                                                                                      \
    case '2':                                                                                                                                                                      \
    case '3':                                                                                                                                                                      \
    case '4':                                                                                                                                                                      \
    case '5':                                                                                                                                                                      \
    case '6':                                                                                                                                                                      \
    case '7':                                                                                                                                                                      \
    case '8':                                                                                                                                                                      \
    case '9': (buf__).push_back(*it); break;

#if ISDEBUG
#define UNREACHABLE()                                                                                                                                                              \
    {                                                                                                                                                                              \
        Log::logger->log(Log::CRIT, "\n\nMEMORY CORRUPTED: Unreachable failed! (Reached an unreachable position, memory corruption!!!)");                                          \
        raise(SIGABRT);                                                                                                                                                            \
        std::unreachable();                                                                                                                                                        \
    }
#else
#define UNREACHABLE() std::unreachable();
#endif

#if ISDEBUG

#define GLCALL(__CALL__)                                                                                                                                                           \
    {                                                                                                                                                                              \
        __CALL__;                                                                                                                                                                  \
        static const auto GLDEBUG = CConfigValue<Config::INTEGER>("debug:gl_debugging");                                                                                           \
        if (*GLDEBUG) {                                                                                                                                                            \
            auto err = glGetError();                                                                                                                                               \
            if (err != GL_NO_ERROR) {                                                                                                                                              \
                Log::logger->log(Log::ERR, "[GLES] Error in call at {}@{}: 0x{:x}", __LINE__,                                                                                      \
                                 ([]() consteval { return std::string_view(__FILE__).substr(std::string_view(__FILE__).find_last_of('/') + 1); })(), err);                         \
            }                                                                                                                                                                      \
        }                                                                                                                                                                          \
    }

#else

#define GLCALL(__CALL__)                                                                                                                                                           \
    { __CALL__; }

#endif

#define HYPRUTILS_FORWARD(ns, name)                                                                                                                                                \
    namespace Hyprutils {                                                                                                                                                          \
        namespace ns {                                                                                                                                                             \
            class name;                                                                                                                                                            \
        }                                                                                                                                                                          \
    }

#define AQUAMARINE_VERSION_NUMBER (AQUAMARINE_VERSION_MAJOR * 10000 + AQUAMARINE_VERSION_MINOR * 100 + AQUAMARINE_VERSION_PATCH)
#define AQUAMARINE_FORWARD(name)                                                                                                                                                   \
    namespace Aquamarine {                                                                                                                                                         \
        class name;                                                                                                                                                                \
    }

#define UNLIKELY(expr) (expr) [[unlikely]]
#define LIKELY(expr)   (expr) [[likely]]
