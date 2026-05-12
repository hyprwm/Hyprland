#pragma once

#include <string>

#include "../SharedDefs.hpp"

namespace Helpers::SystemInfo {
    std::string getSystemInfo();
    std::string getVersion(eHyprCtlOutputFormat fmt);
    std::string getStatus(eHyprCtlOutputFormat fmt);
};