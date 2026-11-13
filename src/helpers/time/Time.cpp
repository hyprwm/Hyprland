#include "Time.hpp"

#define chr std::chrono

using s_ns = std::pair<uint64_t, uint64_t>;

static std::pair<Time::steady_tp, timespec> sampleClocks() {
    const auto before = Time::steadyNow();
    timespec   mono{};
    clock_gettime(CLOCK_MONOTONIC, &mono);
    const auto after = Time::steadyNow();

    return {before + (after - before) / 2, mono};
}

Time::steady_tp Time::steadyNow() {
    return chr::steady_clock::now();
}

Time::system_tp Time::systemNow() {
    return chr::system_clock::now();
}

uint64_t Time::millis(const steady_tp& tp) {
    return chr::duration_cast<chr::milliseconds>(tp.time_since_epoch()).count();
}

s_ns Time::secNsec(const steady_tp& tp) {
    const uint64_t sec     = chr::duration_cast<chr::seconds>(tp.time_since_epoch()).count();
    const auto     nsecdur = tp - chr::steady_clock::time_point(chr::seconds(sec));
    return {sec, chr::duration_cast<chr::nanoseconds>(nsecdur).count()};
}

uint64_t Time::millis(const system_tp& tp) {
    return chr::duration_cast<chr::milliseconds>(tp.time_since_epoch()).count();
}

s_ns Time::secNsec(const system_tp& tp) {
    const uint64_t sec     = chr::duration_cast<chr::seconds>(tp.time_since_epoch()).count();
    const auto     nsecdur = tp - chr::system_clock::time_point(chr::seconds(sec));
    return {sec, chr::duration_cast<chr::nanoseconds>(nsecdur).count()};
}

Time::steady_tp Time::fromTimespec(const timespec* ts) {
    const auto [steady, mono] = sampleClocks();
    const auto diff           = chr::seconds(ts->tv_sec - mono.tv_sec) + chr::nanoseconds(ts->tv_nsec - mono.tv_nsec);

    return steady + chr::duration_cast<steady_tp::duration>(diff);
}

struct timespec Time::toTimespec(const steady_tp& tp) {
    const auto [steady, mono] = sampleClocks();
    const auto time           = chr::seconds(mono.tv_sec) + chr::nanoseconds(mono.tv_nsec) + chr::duration_cast<chr::nanoseconds>(tp - steady);
    const auto sec            = chr::floor<chr::seconds>(time);

    return timespec{
        .tv_sec  = static_cast<time_t>(sec.count()),
        .tv_nsec = static_cast<long>(chr::duration_cast<chr::nanoseconds>(time - sec).count()),
    };
}

Time::steady_dur Time::till(const timespec& ts) {
    timespec mono{};
    clock_gettime(CLOCK_MONOTONIC, &mono);

    return chr::seconds(ts.tv_sec - mono.tv_sec) + chr::nanoseconds(ts.tv_nsec - mono.tv_nsec);
}
