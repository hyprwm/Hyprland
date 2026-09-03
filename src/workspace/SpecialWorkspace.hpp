#pragma once

#include "HLWorkspace.hpp"
#include "../desktop/DesktopTypes.hpp"

namespace Workspace {
    class CSpecialWorkspace : public CHLWorkspace {
      public:
        static PHLWORKSPACE create(PHLMONITOR monitor, std::string address, bool isEmpty = true);
        ~CSpecialWorkspace() override = default;

        CSpecialWorkspace(PHLMONITOR monitor, std::string address, bool isEmpty);
    };
}
