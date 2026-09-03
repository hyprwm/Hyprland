#pragma once

#include "HLWorkspace.hpp"
#include "../desktop/DesktopTypes.hpp"

namespace Workspace {
    class CRegularWorkspace : public CHLWorkspace {
      public:
        static PHLWORKSPACE create(SWorkspaceNumberedID id, PHLMONITOR monitor, std::string name, bool isEmpty = true);
        static PHLWORKSPACE createNamed(PHLMONITOR monitor, std::string address, std::string displayName = {}, bool isEmpty = true);
        ~CRegularWorkspace() override = default;

        void setPersistent(bool persistent);
        bool isPersistent() const;

        CRegularWorkspace(WorkspaceID id, PHLMONITOR monitor, std::string displayName, std::string address, bool isEmpty);

      protected:
        void applyTypeSpecificRules(const Config::CWorkspaceRule& rule) override;

      private:
        PHLWORKSPACE m_selfPersistent;
        bool         m_persistent = false;
    };
}
