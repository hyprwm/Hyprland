#pragma once

#include <chrono>
#include <cstdint>
#include <utility>
#include <ctime>

//NOLINTNEXTLINE
namespace Time {
    using steady_tp  = std::chrono::steady_clock::time_point;
    using system_tp  = std::chrono::system_clock::time_point;
    using steady_dur = std::chrono::steady_clock::duration;
    using system_dur = std::chrono::system_clock::duration;

    steady_tp                     steadyNow();
    system_tp                     systemNow();

    steady_tp                     fromTimespec(const timespec*);
    struct timespec               toTimespec(const steady_tp& tp);

    uint64_t                      millis(const steady_tp& tp);
    uint64_t                      millis(const system_tp& tp);
    std::pair<uint64_t, uint64_t> secNsec(const steady_tp& tp);
    std::pair<uint64_t, uint64_t> secNsec(const system_tp& tp);
};