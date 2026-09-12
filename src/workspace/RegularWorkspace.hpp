#pragma once

#include "HLWorkspace.hpp"
#include "../desktop/DesktopTypes.hpp"

namespace Workspace {
    class CRegularWorkspace : public CHLWorkspace {
      public:
        static PHLWORKSPACE create(SWorkspaceNumberedID id, PHLMONITOR monitor, std::string name);
        static PHLWORKSPACE createNamed(PHLMONITOR monitor, std::string address, std::string displayName = {});
        ~CRegularWorkspace() override = default;

        void setPersistent(bool persistent);
        bool isPersistent() const;

        CRegularWorkspace(WorkspaceID id, PHLMONITOR monitor, std::string displayName, std::string address);

      protected:
        void applyTypeSpecificRules(const Config::CWorkspaceRule& rule) override;

      private:
        PHLWORKSPACE m_selfPersistent;
        bool         m_persistent = false;
    };
}
