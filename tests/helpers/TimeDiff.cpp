#include <gtest/gtest.h>
#include <helpers/time/Time.hpp>

TEST(HelpersTime, DiffBorrowsNanoseconds) {
    const Time::detail::sec_nsec newer{5, 1};
    const Time::detail::sec_nsec older{4, 999'999'999};

    const auto diff = Time::detail::diff(newer, older);

    EXPECT_EQ(diff.first, 0u);
    EXPECT_EQ(diff.second, 2u);
}

TEST(HelpersTime, DiffWithoutBorrow) {
    const Time::detail::sec_nsec newer{3, 500};
    const Time::detail::sec_nsec older{2, 250};

    const auto diff = Time::detail::diff(newer, older);

    EXPECT_EQ(diff.first, 1u);
    EXPECT_EQ(diff.second, 250u);
}

TEST(HelpersTime, NormalizesTimespecNsecOverflow) {
    const Time::detail::sec_nsec raw{1u, 2'000'000'050u};

    const auto normalized = Time::detail::normalize(raw);

    EXPECT_EQ(normalized.first, 3u);
    EXPECT_EQ(normalized.second, 50u);
}

TEST(HelpersTime, SecNsecSteadyFixedPoint) {
    using namespace std::chrono;
    using namespace std::chrono_literals;
    const Time::steady_tp tp{steady_clock::duration{1s + 5000ns}};

    const auto secNsec = Time::secNsec(tp);

    EXPECT_EQ(secNsec.first, 1);
    EXPECT_EQ(secNsec.second, 5000);
}

TEST(HelpersTime, SecNsecSystemFixedPoint) {
    using namespace std::chrono;
    using namespace std::chrono_literals;
    // 5000ns keeps compatibility with coarser system_clock periods (e.g. microseconds).
    const Time::system_tp tp{system_clock::duration{2s + 5000ns}};

    const auto secNsec = Time::secNsec(tp);

    EXPECT_EQ(secNsec.first, 2u);
    EXPECT_EQ(secNsec.second, 5000u);
}
