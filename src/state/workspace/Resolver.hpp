#pragma once

#include "Target.hpp"
#include "../../desktop/DesktopTypes.hpp"

#include <optional>
#include <string>

namespace State::Workspace {
    class CWorkspaceResolver {
      public:
        STarget getWorkspaceTargetFromString(const std::string&, std::optional<PHLMONITOR> = std::nullopt);
    };

    UP<CWorkspaceResolver>& resolver();
}
