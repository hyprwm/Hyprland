#pragma once

#include <vector>
#include <optional>

#include "WorkspaceRule.hpp"
#include "../../../desktop/DesktopTypes.hpp"
#include "../../../helpers/memory/Memory.hpp"

namespace Monitor {
    class IMonitorIdentifiable;
}

namespace Config {
    class CWorkspaceRuleManager {
      public:
        CWorkspaceRuleManager()  = default;
        ~CWorkspaceRuleManager() = default;

        void                                   clear();
        SP<CWorkspaceRule>                     add(CWorkspaceRule&&);
        SP<CWorkspaceRule>                     replaceOrAdd(CWorkspaceRule&&);

        std::optional<CWorkspaceRule>          getWorkspaceRuleFor(PHLWORKSPACE workspace);
        std::string                            getDefaultWorkspaceFor(const Monitor::IMonitorIdentifiable&);
        PHLMONITOR                             getBoundMonitorForWS(const std::string&);
        std::string                            getBoundMonitorStringForWS(const std::string&);
        const std::vector<SP<CWorkspaceRule>>& getAllWorkspaceRules();

      private:
        std::vector<SP<CWorkspaceRule>> m_rules;
    };

    UP<CWorkspaceRuleManager>& workspaceRuleMgr();
};
