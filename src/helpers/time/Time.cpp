#include "Time.hpp"

#define chr                   std::chrono
#define TIMESPEC_NSEC_PER_SEC 1000000000L

using s_ns = std::pair<uint64_t, uint64_t>;

// HAS to be a > b
static s_ns timediff(const s_ns& a, const s_ns& b) {
    s_ns d;

    d.first = a.first - b.first;
    if (a.second >= b.second)
        d.second = a.second - b.second;
    else {
        d.second = TIMESPEC_NSEC_PER_SEC + a.second - b.second;
        d.first -= 1;
    }

    return d;
}

static s_ns timeadd(const s_ns& a, const s_ns& b) {
    s_ns d;

    d.first = a.first + b.first;
    if (a.second + b.second >= TIMESPEC_NSEC_PER_SEC) {
        d.second = a.second + b.second - TIMESPEC_NSEC_PER_SEC;
        d.first += 1;
    } else
        d.second = a.second + b.second;

    return d;
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
    const uint64_t                    sec     = chr::duration_cast<chr::seconds>(tp.time_since_epoch()).count();
    const chr::steady_clock::duration nsecdur = tp - chr::steady_clock::time_point(chr::seconds(sec));
    return std::make_pair<>(sec, chr::duration_cast<chr::nanoseconds>(nsecdur).count());
}

uint64_t Time::millis(const system_tp& tp) {
    return chr::duration_cast<chr::milliseconds>(tp.time_since_epoch()).count();
}

s_ns Time::secNsec(const system_tp& tp) {
    const uint64_t                    sec     = chr::duration_cast<chr::seconds>(tp.time_since_epoch()).count();
    const chr::steady_clock::duration nsecdur = tp - chr::system_clock::time_point(chr::seconds(sec));
    return std::make_pair<>(sec, chr::duration_cast<chr::nanoseconds>(nsecdur).count());
}

// TODO: this is a mess, but C++ doesn't define what steady_clock is.
// At least on Linux, system_clock == CLOCK_REALTIME
// and steady_clock == CLOCK_MONOTONIC,
// or at least it seems so with gcc and gcc's stl.
// but, since we can't *ever* be sure, we have to guess.
// In general, this may shift the time around by a couple hundred ns. Doesn't matter, realistically.

Time::steady_tp Time::fromTimespec(const timespec* ts) {
    struct timespec mono, real;
    clock_gettime(CLOCK_MONOTONIC, &mono);
    clock_gettime(CLOCK_REALTIME, &real);
    Time::steady_tp now    = Time::steadyNow();
    Time::system_tp nowSys = Time::systemNow();
    s_ns            stdSteady, stdReal;
    stdSteady = Time::secNsec(now);
    stdReal   = Time::secNsec(nowSys);

    // timespec difference, REAL - MONO
    s_ns diff = timediff({real.tv_sec, real.tv_nsec}, {mono.tv_sec, mono.tv_nsec});

    // STD difference, REAL - MONO
    s_ns diff2 = timediff(stdReal, stdSteady);

    s_ns diffFinal;
    s_ns monotime = {ts->tv_sec, ts->tv_nsec};

    if (diff.first >= diff2.first || (diff.first == diff2.first && diff.second >= diff2.second))
        diffFinal = timediff(diff, diff2);
    else
        diffFinal = timediff(diff2, diff);

    auto sum = timeadd(monotime, diffFinal);
    return chr::steady_clock::time_point(std::chrono::seconds(sum.first)) + chr::nanoseconds(sum.second);
}

struct timespec Time::toTimespec(const steady_tp& tp) {
    struct timespec mono, real;
    clock_gettime(CLOCK_MONOTONIC, &mono);
    clock_gettime(CLOCK_REALTIME, &real);
    Time::steady_tp now    = Time::steadyNow();
    Time::system_tp nowSys = Time::systemNow();
    s_ns            stdSteady, stdReal;
    stdSteady = Time::secNsec(now);
    stdReal   = Time::secNsec(nowSys);

    // timespec difference, REAL - MONO
    s_ns diff = timediff({real.tv_sec, real.tv_nsec}, {mono.tv_sec, mono.tv_nsec});

    // STD difference, REAL - MONO
    s_ns diff2 = timediff(stdReal, stdSteady);

    s_ns diffFinal;
    s_ns tpTime = secNsec(tp);

    if (diff.first >= diff2.first || (diff.first == diff2.first && diff.second >= diff2.second))
        diffFinal = timediff(diff, diff2);
    else
        diffFinal = timediff(diff2, diff);

    auto sum = timeadd(tpTime, diffFinal);
    return timespec{.tv_sec = sum.first, .tv_nsec = sum.second};
}

namespace Time::detail {

sec_nsec diff(const sec_nsec& newer, const sec_nsec& older) {
    return timediff(newer, older);
}

}
