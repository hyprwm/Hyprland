#pragma once

#include <vector>
#include <optional>

#include "WorkspaceRule.hpp"
#include "../../../desktop/DesktopTypes.hpp"
#include "../../../helpers/memory/Memory.hpp"
#include "../../../workspace/AbstractWorkspace.hpp"

namespace Monitor {
    class IMonitorAddressable;
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
        std::string                            getDefaultWorkspaceFor(const Monitor::IMonitorAddressable&);
        PHLMONITOR                             getBoundMonitorForWS(const std::string&);
        std::string                            getBoundMonitorStringForWS(const std::string&);
        std::optional<PHLMONITOR>              getBoundMonitorForWS(const ::Workspace::WorkspaceID&, ::Workspace::eWorkspaceType, std::string_view address);
        std::string                            getBoundMonitorStringForWS(const ::Workspace::WorkspaceID&, ::Workspace::eWorkspaceType, std::string_view address);
        const std::vector<SP<CWorkspaceRule>>& getAllWorkspaceRules();

      private:
        std::vector<SP<CWorkspaceRule>> m_rules;
    };

    UP<CWorkspaceRuleManager>& workspaceRuleMgr();
};
