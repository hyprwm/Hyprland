#pragma once
#include "Log.hpp"
#include "tests/shared.hpp"

#include <cmath>
#include <map>
#include <memory>
#include <string>

// TODO: localize these global variables
inline std::string HIS       = "";
inline std::string WLDISPLAY = "";

// =================================
//      TEST CASES DEFINITION
// =================================

class CTestCase {
  public:
    CTestCase()                 = default;
    CTestCase(const CTestCase&) = delete; // Test cases probably should not be copied
    virtual ~CTestCase()        = default;

    /// Indicates that some check has failed
    bool failed = false;
    /// Indicates that the _last_ check has failed
    bool just_failed = false;

    /// Run the test. The test result will be stored in `.failed`.
    virtual void test() = 0;

    // TODO: provide an adequate API. For instance, make all the members above private/protected,
    // and expose a method that will return a bool indicating test success.

    /// Test name, as defined in the source file
    virtual std::string name() const = 0;

    /// Name of the source file where the test is defined
    virtual std::string filename() const = 0;

    /// Test group name (defined in tests/*/tests.hpp files)
    virtual std::string groupName() const = 0;
};

// Methods in the generated definition are marked with `[[maybe_unused]]` to suppress warnings.
// That's because:
// 1. As of this writing, some are indeed unused. They are there for future convenience.
// 2. The rest are only used behind a `std::shared_ptr` but my compiler does not detect it.
#define TEST_CASE(NAME)                                                                                                                                                            \
    namespace {                                                                                                                                                                    \
        class TestCase_##NAME : public CTestCase {                                                                                                                                 \
          public:                                                                                                                                                                  \
            void        test() override;                                                                                                                                           \
            std::string name() const override;                                                                                                                                     \
            std::string filename() const override;                                                                                                                                 \
            std::string groupName() const override;                                                                                                                                \
        };                                                                                                                                                                         \
    }                                                                                                                                                                              \
                                                                                                                                                                                   \
    static auto test_case_##NAME          = std::make_shared<TestCase_##NAME>();                                                                                                   \
    static auto register_test_case_##NAME = [] {                                                                                                                                   \
        /* Common test storage used when running selected tests (declared below) */                                                                                                \
        testCases.emplace(#NAME, test_case_##NAME);                                                                                                                                \
        /* Group-specific test storage used when running all tests (declared by our includer) */                                                                                   \
        GROUP_TEST_CASE_STORAGE.push_back(test_case_##NAME);                                                                                                                       \
        return 1;                                                                                                                                                                  \
    }();                                                                                                                                                                           \
                                                                                                                                                                                   \
    [[maybe_unused]]                                                                                                                                                               \
    std::string TestCase_##NAME::name() const {                                                                                                                                    \
        return #NAME;                                                                                                                                                              \
    }                                                                                                                                                                              \
                                                                                                                                                                                   \
    [[maybe_unused]]                                                                                                                                                               \
    std::string TestCase_##NAME::filename() const {                                                                                                                                \
        return __FILE__;                                                                                                                                                           \
    }                                                                                                                                                                              \
                                                                                                                                                                                   \
    [[maybe_unused]]                                                                                                                                                               \
    std::string TestCase_##NAME::groupName() const {                                                                                                                               \
        /* Defined by our includer */                                                                                                                                              \
        return TEST_GROUP_NAME;                                                                                                                                                    \
    }                                                                                                                                                                              \
                                                                                                                                                                                   \
    [[maybe_unused]]                                                                                                                                                               \
    void TestCase_##NAME::test()

#define SUBTEST(NAME, ...)                                                                                                                                                         \
    namespace {                                                                                                                                                                    \
        class Subtest_##NAME {                                                                                                                                                     \
          public:                                                                                                                                                                  \
            bool failed      = false;                                                                                                                                              \
            bool just_failed = false;                                                                                                                                              \
                                                                                                                                                                                   \
            void main(__VA_ARGS__);                                                                                                                                                \
        };                                                                                                                                                                         \
    }                                                                                                                                                                              \
                                                                                                                                                                                   \
    void Subtest_##NAME::main(__VA_ARGS__)

#define CALL_SUBTEST(NAME, ...)                                                                                                                                                    \
    do {                                                                                                                                                                           \
        auto subtest_##NAME = Subtest_##NAME{};                                                                                                                                    \
        subtest_##NAME.main(__VA_ARGS__);                                                                                                                                          \
        if (subtest_##NAME.failed) {                                                                                                                                               \
            FAIL_TEST("Subtest {}({}) failed", #NAME, #__VA_ARGS__);                                                                                                               \
        } else {                                                                                                                                                                   \
            LOG_OK("Subtest {}({})", #NAME, #__VA_ARGS__);                                                                                                                         \
        }                                                                                                                                                                          \
    } while (0)

// =================================
//    DECLARAITIONS USED BY TESTS
// =================================

/// Stores all test cases regardless of their place of definition.
inline std::map<std::string, std::shared_ptr<CTestCase>> testCases;

// =================================
//           IN-TEST MACROS
// =================================

/// Marks the test as failed without terminating it
#define MARK_TEST_FAILED_SILENT() this->just_failed = this->failed = true

/// Prints a failure message and makrs the test as failed without terminating it
#define MARK_TEST_FAILED(fmt, ...)                                                                                                                                                 \
    do {                                                                                                                                                                           \
        NLog::red("Failed:{} " fmt ". Source: {}@{}.", Colors::RESET __VA_OPT__(, ) __VA_ARGS__, __FILE__, __LINE__);                                                              \
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
        this->just_failed = false;                                                                                                                                                 \
        NLog::green("OK:{} " fmt ". Source: {}@{}", Colors::RESET, __VA_ARGS__, __FILE__, __LINE__);                                                                               \
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
        NLog::red("Failed: {}{} should contain {} but doesn't. Source: {}@{}. Haystack is:\n{}", Colors::RESET, #haystack, #needle, __FILE__, __LINE__, std::string{haystack});    \
        MARK_TEST_FAILED_SILENT();                                                                                                                                                 \
    } else {                                                                                                                                                                       \
        LOG_OK("{} contains {}.", #haystack, #needle);                                                                                                                             \
    }

#define EXPECT_NOT_CONTAINS(haystack, needle)                                                                                                                                      \
    if (std::string{haystack}.contains(needle)) {                                                                                                                                  \
        NLog::red("Failed: {}{} shouldn't contain {} but does. Source: {}@{}. Haystack is:\n{}", Colors::RESET, #haystack, #needle, __FILE__, __LINE__, std::string{haystack});    \
        MARK_TEST_FAILED_SILENT();                                                                                                                                                 \
    } else {                                                                                                                                                                       \
        LOG_OK("{} doesn't contain {}.", #haystack, #needle);                                                                                                                      \
    }

#define EXPECT_STARTS_WITH(str, what)                                                                                                                                              \
    if (!std::string{str}.starts_with(what)) {                                                                                                                                     \
        NLog::red("Failed: {}{} should start with {} but doesn't. Source: {}@{}. String is:\n{}", Colors::RESET, #str, #what, __FILE__, __LINE__, std::string{str});               \
        MARK_TEST_FAILED_SILENT();                                                                                                                                                 \
    } else {                                                                                                                                                                       \
        LOG_OK("{} starts with {}.", #str, #what);                                                                                                                                 \
    }

#define EXPECT_COUNT_STRING(str, what, no)                                                                                                                                         \
    if (Tests::countOccurrences(str, what) != no) {                                                                                                                                \
        NLog::red("Failed: {}{} should contain {} {} times, but doesn't. Source: {}@{}. String is:\n{}", Colors::RESET, #str, #what, no, __FILE__, __LINE__, std::string{str});    \
        MARK_TEST_FAILED_SILENT();                                                                                                                                                 \
    } else {                                                                                                                                                                       \
        LOG_OK("{} contains {} {} times.", #str, #what, no);                                                                                                                       \
    }

#define EXPECT_OK(x) EXPECT(x, "ok")

#define ASSERT_MAX_DELTA(expr, desired, delta)                                                                                                                                     \
    do {                                                                                                                                                                           \
        EXPECT_MAX_DELTA(expr, desired, delta);                                                                                                                                    \
        if (this->just_failed)                                                                                                                                                     \
            return;                                                                                                                                                                \
    } while (0)

#define ASSERT(expr, val)                                                                                                                                                          \
    do {                                                                                                                                                                           \
        EXPECT(expr, val);                                                                                                                                                         \
        if (this->just_failed)                                                                                                                                                     \
            return;                                                                                                                                                                \
    } while (0)

#define ASSERT_NOT(expr, val)                                                                                                                                                      \
    do {                                                                                                                                                                           \
        EXPECT_NOT(expr, val);                                                                                                                                                     \
        if (this->just_failed)                                                                                                                                                     \
            return;                                                                                                                                                                \
    } while (0)

#define ASSERT_VECTOR2D(expr, val)                                                                                                                                                 \
    do {                                                                                                                                                                           \
        EXPECT_VECTOR2D(expr, val);                                                                                                                                                \
        if (this->just_failed)                                                                                                                                                     \
            return;                                                                                                                                                                \
    } while (0)

#define ASSERT_CONTAINS(haystack, needle)                                                                                                                                          \
    do {                                                                                                                                                                           \
        EXPECT_CONTAINS(haystack, needle);                                                                                                                                         \
        if (this->just_failed)                                                                                                                                                     \
            return;                                                                                                                                                                \
    } while (0)

#define ASSERT_NOT_CONTAINS(haystack, needle)                                                                                                                                      \
    do {                                                                                                                                                                           \
        EXPECT_NOT_CONTAINS(haystack, needle);                                                                                                                                     \
        if (this->just_failed)                                                                                                                                                     \
            return;                                                                                                                                                                \
    } while (0)

#define ASSERT_STARTS_WITH(str, what)                                                                                                                                              \
    do {                                                                                                                                                                           \
        EXPECT_STARTS_WITH(str, what);                                                                                                                                             \
        if (this->just_failed)                                                                                                                                                     \
            return;                                                                                                                                                                \
    } while (0)

#define ASSERT_COUNT_STRING(str, what, no)                                                                                                                                         \
    do {                                                                                                                                                                           \
        EXPECT_COUNT_STRING(str, what, no);                                                                                                                                        \
        if (this->just_failed)                                                                                                                                                     \
            return;                                                                                                                                                                \
    } while (0)

#define SPAWN_KITTY(class_, ...)                                                                                                                                                   \
    do {                                                                                                                                                                           \
        if (!Tests::spawnKitty(class_ __VA_OPT__(, ) __VA_ARGS__))                                                                                                                 \
            FAIL_TEST("Could not spawn kitty with class: {}", class_);                                                                                                             \
    } while (0)

#define SPAWN_LAYER_KITTY(class_, ...)                                                                                                                                             \
    do {                                                                                                                                                                           \
        if (!Tests::spawnLayerKitty(class_ __VA_OPT__(, ) __VA_ARGS__))                                                                                                            \
            FAIL_TEST("Could not spawn layer kitty with class: {}", class_);                                                                                                       \
    } while (0)

#define OK(x)  ASSERT(x, "ok")
#define NOK(x) ASSERT_NOT(x, "ok")
