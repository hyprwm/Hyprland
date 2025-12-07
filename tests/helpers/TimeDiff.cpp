#include <gtest/gtest.h>
#include <helpers/time/Time.hpp>

TEST(HelpersTime, DiffBorrowsNanoseconds) {
    const Time::detail::sec_nsec newer{5, 1};
    const Time::detail::sec_nsec older{4, 999999999};

    const auto diff = Time::detail::diff(newer, older);

    EXPECT_EQ(diff.first, 0);
    EXPECT_EQ(diff.second, 2);
}

TEST(HelpersTime, DiffWithoutBorrow) {
    const Time::detail::sec_nsec newer{3, 500};
    const Time::detail::sec_nsec older{2, 250};

    const auto diff = Time::detail::diff(newer, older);

    EXPECT_EQ(diff.first, 1);
    EXPECT_EQ(diff.second, 250);
}

TEST(HelpersTime, SecNsecSteadyFixedPoint) {
    using namespace std::chrono;
    const Time::steady_tp tp{steady_clock::duration{seconds(1) + nanoseconds(5000)}};

    const auto secNsec = Time::secNsec(tp);

    EXPECT_EQ(secNsec.first, 1);
    EXPECT_EQ(secNsec.second, 5000);
}

TEST(HelpersTime, SecNsecSystemFixedPoint) {
    using namespace std::chrono;
    // 5000ns keeps compatibility with coarser system_clock periods (e.g. microseconds).
    const Time::system_tp tp{system_clock::duration{seconds(2) + nanoseconds(5000)}};

    const auto secNsec = Time::secNsec(tp);

    EXPECT_EQ(secNsec.first, 2);
    EXPECT_EQ(secNsec.second, 5000);
}
