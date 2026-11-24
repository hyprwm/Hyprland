#pragma once

#include <string_view>

namespace CrashReporter {
    void createAndSaveCrash(int sig);
    void setAssertReason(std::string_view reason);
};
