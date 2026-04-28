// Stolen from hyprutils

#pragma once
#include <cmath>
#include <string>

// TODO: localize these global variables
inline std::string HIS       = "";
inline std::string WLDISPLAY = "";

namespace Colors {
    constexpr const char* RED     = "\x1b[31m";
    constexpr const char* GREEN   = "\x1b[32m";
    constexpr const char* YELLOW  = "\x1b[33m";
    constexpr const char* BLUE    = "\x1b[34m";
    constexpr const char* MAGENTA = "\x1b[35m";
    constexpr const char* CYAN    = "\x1b[36m";
    constexpr const char* RESET   = "\x1b[0m";
};

// =================================
//      TEST CASES DEFINITION
// =================================

class CTestCase {
  public:
    CTestCase()                 = default;
    CTestCase(const CTestCase&) = delete; // Test cases probably should not be copied
    bool failed                 = false;
    virtual ~CTestCase()        = default;

    // TODO: `test` will be protected
    virtual void test() = 0;
};

#define TEST_CASE(name)                                                                                                                                                            \
    namespace {                                                                                                                                                                    \
        class TestCase_##name : public CTestCase {                                                                                                                                 \
          public:                                                                                                                                                                  \
            void test() override;                                                                                                                                                  \
        };                                                                                                                                                                         \
    }                                                                                                                                                                              \
                                                                                                                                                                                   \
    static TestCase_##name test_case_##name{};                                                                                                                                     \
    static auto            register_test_case_##name = [] {                                                                                                                        \
        /* `TEST_CASES_STORAGE` must be defined by the caller */                                                                                                                   \
        TEST_CASES_STORAGE.emplace(#name, test_case_##name);                                                                                                                       \
        return 1;                                                                                                                                                                  \
    }();                                                                                                                                                                           \
                                                                                                                                                                                   \
    void TestCase_##name::test()

#define SUBTEST(name, ...)                                                                                                                                                         \
    namespace {                                                                                                                                                                    \
        class Subtest_##name {                                                                                                                                                     \
          public:                                                                                                                                                                  \
            bool failed = false;                                                                                                                                                   \
                                                                                                                                                                                   \
            void main(__VA_ARGS__);                                                                                                                                                \
        };                                                                                                                                                                         \
    }                                                                                                                                                                              \
                                                                                                                                                                                   \
    void Subtest_##name::main(__VA_ARGS__)

#define CALL_SUBTEST(name, ...)                                                                                                                                                    \
    do {                                                                                                                                                                           \
        auto subtest_##name = Subtest_##name{};                                                                                                                                    \
        subtest_##name.main(__VA_ARGS__);                                                                                                                                          \
        if (subtest_##name.failed) {                                                                                                                                               \
            NLog::log("{}Subtest {}({}) failed", Colors::RED, #name, #__VA_ARGS__);                                                                                                \
            this->failed = true;                                                                                                                                                   \
            return;                                                                                                                                                                \
        }                                                                                                                                                                          \
    } while (0)

// =================================
//           IN-TEST MACROS
// =================================

/// Marks the test as failed without terminating it
#define MARK_TEST_FAILED_SILENT() this->failed = true

/// Prints a failure message and makrs the test as failed without terminating it
#define MARK_TEST_FAILED(fmt, ...)                                                                                                                                                 \
    do {                                                                                                                                                                           \
        NLog::log("{}Failed:{} " fmt ". Source: {}@{}.", Colors::RED, Colors::RESET __VA_OPT__(, ) __VA_ARGS__, __FILE__, __LINE__);                                               \
        MARK_TEST_FAILED_SILENT();                                                                                                                                                 \
    } while (0)

/// Terminates the test execution and marks it as failed
#define FAIL_TEST_SILENT()                                                                                                                                                         \
    do {                                                                                                                                                                           \
        MARK_TEST_FAILED_SILENT();                                                                                                                                                 \
        return;                                                                                                                                                                    \
    } while (0)

/// Prints a failure message, terminates the test execution, and marks it as failed
#define FAIL_TEST(fmt, ...)                                                                                                                                                        \
    do {                                                                                                                                                                           \
        MARK_TEST_FAILED(fmt, __VA_ARGS__);                                                                                                                                        \
        return;                                                                                                                                                                    \
    } while (0)

#define LOG_OK(fmt, ...)                                                                                                                                                           \
    do {                                                                                                                                                                           \
        NLog::log("{}OK:{} " fmt ". Source: {}@{}", Colors::GREEN, Colors::RESET, __VA_ARGS__, __FILE__, __LINE__);                                                                \
    } while (0)

// In case of failure:
// - All the `EXPECT*` macros will print a message and mark the test as failed without terminating its execution;
// - All the `ASSERT*` macros will print a message, mark the test as failed, and terminate it.
//
// `OK` acts like `ASSERT_OK`.

#define EXPECT_MAX_DELTA(expr, desired, delta)                                                                                                                                     \
    if (const auto RESULT = expr; std::abs(RESULT - (desired)) > delta) {                                                                                                          \
        MARK_TEST_FAILED("{}, expected max delta of {}, got delta {} ({} - {})", #expr, delta, (RESULT - (desired)), RESULT, desired);                                             \
    } else {                                                                                                                                                                       \
        LOG_OK("{}. Got {}", #expr, (RESULT - (desired)));                                                                                                                         \
    }

#define EXPECT(expr, val)                                                                                                                                                          \
    do {                                                                                                                                                                           \
        if (const auto RESULT = expr; RESULT != (val)) {                                                                                                                           \
            MARK_TEST_FAILED("{}, expected {}, got {}", #expr, val, RESULT);                                                                                                       \
        } else {                                                                                                                                                                   \
            LOG_OK("{}. Got {}", #expr, val);                                                                                                                                      \
        }                                                                                                                                                                          \
    } while (0)

#define EXPECT_NOT(expr, val)                                                                                                                                                      \
    if (const auto RESULT = expr; RESULT == (val)) {                                                                                                                               \
        MARK_TEST_FAILED("{}, expected not {}, got {}", #expr, val, RESULT);                                                                                                       \
    } else {                                                                                                                                                                       \
        LOG_OK("{}. Got {}", #expr, val);                                                                                                                                          \
    }

#define EXPECT_VECTOR2D(expr, val)                                                                                                                                                 \
    do {                                                                                                                                                                           \
        const auto& RESULT   = expr;                                                                                                                                               \
        const auto& ASSERTED = val;                                                                                                                                                \
        if (!(std::abs(RESULT.x - ASSERTED.x) < 1e-6 && std::abs(RESULT.y - ASSERTED.y) < 1e-6)) {                                                                                 \
            MARK_TEST_FAILED("{}, expected [{}, {}], got [{}, {}]", #expr, ASSERTED.x, ASSERTED.y, RESULT.x, RESULT.y);                                                            \
        } else {                                                                                                                                                                   \
            LOG_OK("{}. Got [{}, {}].", #expr, RESULT.x, RESULT.y);                                                                                                                \
        }                                                                                                                                                                          \
    } while (0)

// String check macros below use a special error message layout, putting the Source reference before `haystack`,
// since `haystack` may be a large multi-line string. Thus, they use bare `NLog::log` + `MARK_TEST_FAILED_SILENT`.

#define EXPECT_CONTAINS(haystack, needle)                                                                                                                                          \
    if (const auto ASSERTED = needle; !std::string{haystack}.contains(ASSERTED)) {                                                                                                 \
        NLog::log("{}Failed: {}{} should contain {} but doesn't. Source: {}@{}. Haystack is:\n{}", Colors::RED, Colors::RESET, #haystack, #needle, __FILE__, __LINE__,             \
                  std::string{haystack});                                                                                                                                          \
        MARK_TEST_FAILED_SILENT();                                                                                                                                                 \
    } else {                                                                                                                                                                       \
        LOG_OK("{} contains {}.", #haystack, #needle);                                                                                                                            \
    }

#define EXPECT_NOT_CONTAINS(haystack, needle)                                                                                                                                      \
    if (std::string{haystack}.contains(needle)) {                                                                                                                                  \
        NLog::log("{}Failed: {}{} shouldn't contain {} but does. Source: {}@{}. Haystack is:\n{}", Colors::RED, Colors::RESET, #haystack, #needle, __FILE__, __LINE__,             \
                  std::string{haystack});                                                                                                                                          \
        MARK_TEST_FAILED_SILENT();                                                                                                                                                 \
    } else {                                                                                                                                                                       \
        LOG_OK("{} doesn't contain {}.", #haystack, #needle);                                                                                                                      \
    }

#define EXPECT_STARTS_WITH(str, what)                                                                                                                                              \
    if (!std::string{str}.starts_with(what)) {                                                                                                                                     \
        NLog::log("{}Failed: {}{} should start with {} but doesn't. Source: {}@{}. String is:\n{}", Colors::RED, Colors::RESET, #str, #what, __FILE__, __LINE__,                   \
                  std::string{str});                                                                                                                                               \
        MARK_TEST_FAILED_SILENT();                                                                                                                                                 \
    } else {                                                                                                                                                                       \
        LOG_OK("{} starts with {}.", #str, #what);                                                                                                                                 \
    }

#define EXPECT_COUNT_STRING(str, what, no)                                                                                                                                         \
    if (Tests::countOccurrences(str, what) != no) {                                                                                                                                \
        NLog::log("{}Failed: {}{} should contain {} {} times, but doesn't. Source: {}@{}. String is:\n{}", Colors::RED, Colors::RESET, #str, #what, no, __FILE__, __LINE__,        \
                  std::string{str});                                                                                                                                               \
        MARK_TEST_FAILED_SILENT();                                                                                                                                                 \
    } else {                                                                                                                                                                       \
        LOG_OK("{} contains {} {} times.", #str, #what, no);                                                                                                                       \
    }

#define EXPECT_OK(x) EXPECT(x, "ok")

#define ASSERT_MAX_DELTA(expr, desired, delta)                                                                                                                                     \
    do {                                                                                                                                                                           \
        EXPECT_MAX_DELTA(expr, desired, delta);                                                                                                                                    \
        if (this->failed)                                                                                                                                                          \
            return;                                                                                                                                                                \
    } while (0)

#define ASSERT(expr, val)                                                                                                                                                          \
    do {                                                                                                                                                                           \
        EXPECT(expr, val);                                                                                                                                                         \
        if (this->failed)                                                                                                                                                          \
            return;                                                                                                                                                                \
    } while (0)

#define ASSERT_NOT(expr, val)                                                                                                                                                      \
    do {                                                                                                                                                                           \
        EXPECT_NOT(expr, val);                                                                                                                                                     \
        if (this->failed)                                                                                                                                                          \
            return;                                                                                                                                                                \
    } while (0)

#define ASSERT_VECTOR2D(expr, val)                                                                                                                                                 \
    do {                                                                                                                                                                           \
        EXPECT_VECTOR2D(expr, val);                                                                                                                                                \
        if (this->failed)                                                                                                                                                          \
            return;                                                                                                                                                                \
    } while (0)

#define ASSERT_CONTAINS(haystack, needle)                                                                                                                                          \
    do {                                                                                                                                                                           \
        EXPECT_CONTAINS(haystack, needle);                                                                                                                                         \
        if (this->failed)                                                                                                                                                          \
            return;                                                                                                                                                                \
    } while (0)

#define ASSERT_NOT_CONTAINS(haystack, needle)                                                                                                                                      \
    do {                                                                                                                                                                           \
        EXPECT_NOT_CONTAINS(haystack, needle);                                                                                                                                     \
        if (this->failed)                                                                                                                                                          \
            return;                                                                                                                                                                \
    } while (0)

#define ASSERT_STARTS_WITH(str, what)                                                                                                                                              \
    do {                                                                                                                                                                           \
        EXPECT_STARTS_WITH(str, what);                                                                                                                                             \
        if (this->failed)                                                                                                                                                          \
            return;                                                                                                                                                                \
    } while (0)

#define ASSERT_COUNT_STRING(str, what, no)                                                                                                                                         \
    do {                                                                                                                                                                           \
        EXPECT_COUNT_STRING(str, what, no);                                                                                                                                        \
        if (this->failed)                                                                                                                                                          \
            return;                                                                                                                                                                \
    } while (0)

#define OK(x) ASSERT(x, "ok")
