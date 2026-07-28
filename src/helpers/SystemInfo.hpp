#pragma once

#include <string>

#include "../ipc/s1/S1.hpp"

namespace Helpers::SystemInfo {
    std::string getSystemInfo();
    std::string getVersion(IPC::Socket1::eOutputFormat fmt);
    std::string getStatus(IPC::Socket1::eOutputFormat fmt);
};
