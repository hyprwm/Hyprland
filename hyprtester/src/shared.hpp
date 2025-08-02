// Stolen from hyprutils

#pragma once
#include <iostream>

inline std::string HIS          = "";
inline std::string WLDISPLAY    = "";
inline int         TESTS_PASSED = 0;
inline int         TESTS_FAILED = 0;

namespace Colors {
    constexpr const char* RED     = "\x1b[31m";
    constexpr const char* GREEN   = "\x1b[32m";
    constexpr const char* YELLOW  = "\x1b[33m";
    constexpr const char* BLUE    = "\x1b[34m";
    constexpr const char* MAGENTA = "\x1b[35m";
    constexpr const char* CYAN    = "\x1b[36m";
    constexpr const char* RESET   = "\x1b[0m";
};

#define EXPECT(expr, val)                                                                                                                                                          \
    if (const auto RESULT = expr; RESULT != (val)) {                                                                                                                               \
        NLog::log("{}Failed: {}{}, expected {}, got {}. Source: {}@{}.", Colors::RED, Colors::RESET, #expr, val, RESULT, __FILE__, __LINE__);                                      \
        ret = 1;                                                                                                                                                                   \
        TESTS_FAILED++;                                                                                                                                                            \
    } else {                                                                                                                                                                       \
        NLog::log("{}Passed: {}{}. Got {}", Colors::GREEN, Colors::RESET, #expr, val);                                                                                             \
        TESTS_PASSED++;                                                                                                                                                            \
    }

#define EXPECT_VECTOR2D(expr, val)                                                                                                                                                 \
    do {                                                                                                                                                                           \
        const auto& RESULT   = expr;                                                                                                                                               \
        const auto& EXPECTED = val;                                                                                                                                                \
        if (!(std::abs(RESULT.x - EXPECTED.x) < 1e-6 && std::abs(RESULT.y - EXPECTED.y) < 1e-6)) {                                                                                 \
            NLog::log("{}Failed: {}{}, expected [{}, {}], got [{}, {}]. Source: {}@{}.", Colors::RED, Colors::RESET, #expr, EXPECTED.x, EXPECTED.y, RESULT.x, RESULT.y, __FILE__,  \
                      __LINE__);                                                                                                                                                   \
            ret = 1;                                                                                                                                                               \
            TESTS_FAILED++;                                                                                                                                                        \
        } else {                                                                                                                                                                   \
            NLog::log("{}Passed: {}{}. Got [{}, {}].", Colors::GREEN, Colors::RESET, #expr, RESULT.x, RESULT.y);                                                                   \
            TESTS_PASSED++;                                                                                                                                                        \
        }                                                                                                                                                                          \
    } while (0)

#define EXPECT_CONTAINS(haystack, needle)                                                                                                                                          \
    if (const auto EXPECTED = needle; !std::string{haystack}.contains(EXPECTED)) {                                                                                                 \
        NLog::log("{}Failed: {}{} should contain {} but doesn't. Source: {}@{}. Haystack is:\n{}", Colors::RED, Colors::RESET, #haystack, #needle, __FILE__, __LINE__,             \
                  std::string{haystack});                                                                                                                                          \
        ret = 1;                                                                                                                                                                   \
        TESTS_FAILED++;                                                                                                                                                            \
    } else {                                                                                                                                                                       \
        NLog::log("{}Passed: {}{} contains {}.", Colors::GREEN, Colors::RESET, #haystack, EXPECTED);                                                                               \
        TESTS_PASSED++;                                                                                                                                                            \
    }

#define EXPECT_NOT_CONTAINS(haystack, needle)                                                                                                                                      \
    if (std::string{haystack}.contains(needle)) {                                                                                                                                  \
        NLog::log("{}Failed: {}{} shouldn't contain {} but does. Source: {}@{}. Haystack is:\n{}", Colors::RED, Colors::RESET, #haystack, #needle, __FILE__, __LINE__,             \
                  std::string{haystack});                                                                                                                                          \
        ret = 1;                                                                                                                                                                   \
        TESTS_FAILED++;                                                                                                                                                            \
    } else {                                                                                                                                                                       \
        NLog::log("{}Passed: {}{} doesn't contain {}.", Colors::GREEN, Colors::RESET, #haystack, #needle);                                                                         \
        TESTS_PASSED++;                                                                                                                                                            \
    }

#define EXPECT_STARTS_WITH(str, what)                                                                                                                                              \
    if (!std::string{str}.starts_with(what)) {                                                                                                                                     \
        NLog::log("{}Failed: {}{} should start with {} but doesn't. Source: {}@{}. String is:\n{}", Colors::RED, Colors::RESET, #str, #what, __FILE__, __LINE__,                   \
                  std::string{str});                                                                                                                                               \
        ret = 1;                                                                                                                                                                   \
        TESTS_FAILED++;                                                                                                                                                            \
    } else {                                                                                                                                                                       \
        NLog::log("{}Passed: {}{} starts with {}.", Colors::GREEN, Colors::RESET, #str, #what);                                                                                    \
        TESTS_PASSED++;                                                                                                                                                            \
    }

#define EXPECT_COUNT_STRING(str, what, no)                                                                                                                                         \
    if (Tests::countOccurrences(str, what) != no) {                                                                                                                                \
        NLog::log("{}Failed: {}{} should contain {} {} times, but doesn't. Source: {}@{}. String is:\n{}", Colors::RED, Colors::RESET, #str, #what, no, __FILE__, __LINE__,        \
                  std::string{str});                                                                                                                                               \
        ret = 1;                                                                                                                                                                   \
        TESTS_FAILED++;                                                                                                                                                            \
    } else {                                                                                                                                                                       \
        NLog::log("{}Passed: {}{} contains {} {} times.", Colors::GREEN, Colors::RESET, #str, #what, no);                                                                          \
        TESTS_PASSED++;                                                                                                                                                            \
    }

#define OK(x) EXPECT(x, "ok")
#define FIXME(code)                                                                                                                                                                \
    {                                                                                                                                                                              \
        const int OLD_FAILED = TESTS_FAILED;                                                                                                                                       \
        const int OLD_RET    = ret;                                                                                                                                                \
                                                                                                                                                                                   \
        { code }                                                                                                                                                                   \
                                                                                                                                                                                   \
        if (TESTS_FAILED > OLD_FAILED) {                                                                                                                                           \
            NLog::log("{}FIXME Broken test has failed, counting as passed", Colors::YELLOW);                                                                                       \
            TESTS_FAILED--;                                                                                                                                                        \
            TESTS_PASSED++;                                                                                                                                                        \
            ret = OLD_RET;                                                                                                                                                         \
        } else {                                                                                                                                                                   \
            NLog::log("{}FIXME Broken test has passed, change it to EXPECT", Colors::YELLOW);                                                                                      \
            TESTS_FAILED++;                                                                                                                                                        \
            TESTS_PASSED--;                                                                                                                                                        \
            ret = 1;                                                                                                                                                               \
        }                                                                                                                                                                          \
    }