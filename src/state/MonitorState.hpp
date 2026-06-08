#pragma once

#include <vector>
#include <unordered_map>
#include <aquamarine/output/Output.hpp>

#include "../desktop/DesktopTypes.hpp"
#include "../helpers/signal/Signal.hpp"
#include "../SharedDefs.hpp"

namespace State {
    class CMonitorStateTracker {
      public:
        CMonitorStateTracker();
        ~CMonitorStateTracker() = default;

        const std::vector<PHLMONITOR>& allMonitors() const;
        const std::vector<PHLMONITOR>& monitors() const;

        void                           add(PHLMONITOR mon);
        void                           add(SP<Aquamarine::IOutput> output);
        void                           remove(PHLMONITOR mon);

        void                           finish();

      private:
        MONITORID                                  getNextAvailableMonitorID(const std::string& name);

        std::vector<PHLMONITOR>                    m_realMonitors;
        std::vector<PHLMONITOR>                    m_monitors;

        std::unordered_map<std::string, MONITORID> m_monitorIDMap;

        struct {
            CHyprSignalListener monitorAdded;
            CHyprSignalListener monitorRemoved;
            CHyprSignalListener layoutChanged;
        } m_listeners;
    };

    UP<CMonitorStateTracker>& monitorState();
}