#include <gtest/gtest.h>
#include <helpers/time/Time.hpp>
#include <chrono>

TEST(HelpersTime, HandlesNanosecondBorrowInDiff) {
    const Time::detail::sec_nsec later{5, 5};
    const Time::detail::sec_nsec earlier{4, 999'999'900};

    const auto delta = Time::detail::diff(later, earlier);

    EXPECT_EQ(delta.first, 0u);
    EXPECT_EQ(delta.second, 105u);
}

TEST(HelpersTime, SteadyRoundtripWithinTolerance) {
    using namespace std::chrono_literals;

    const auto now        = Time::steadyNow();
    const auto ts         = Time::toTimespec(now);
    const auto roundtrip  = Time::fromTimespec(&ts);
    const auto diff       = roundtrip > now ? (roundtrip - now) : (now - roundtrip);
    const auto diffMillis = std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();

    // Allow a small drift for conversions and scheduling jitter.
    EXPECT_LT(diffMillis, 50) << "Roundtrip drift exceeded tolerance";
}
