#pragma once

#include <format>
#include <string>
#include <string_view>

namespace Workspace {
    inline std::string specialWorkspaceAddressFromName(std::string_view sv) {
        return sv.empty() ? "special:special" : std::format("special:{}", sv);
    }
}
