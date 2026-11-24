#pragma once

#include <string_view>

namespace CrashReporter {
    // sigReturnAddr is the signal handler's return address, used to find the frame interrupted by the signal
    void createAndSaveCrash(int sig, const void* sigReturnAddr);
    void setAssertReason(std::string_view reason);
};
