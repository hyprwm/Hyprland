#pragma once

#include <vector>
#include <optional>

#include "MonitorRule.hpp"
#include "../../../desktop/DesktopTypes.hpp"
#include "../../../helpers/memory/Memory.hpp"
#include "../../../helpers/signal/Signal.hpp"

namespace Config {
    class CMonitorRuleManager {
      public:
        CMonitorRuleManager();
        ~CMonitorRuleManager() = default;

        void                             clear();
        void                             add(CMonitorRule&&);

        void                             scheduleReload();

        void                             ensureMonitorStatus();
        void                             ensureVRR(PHLMONITOR pMonitor = nullptr);

        CMonitorRule                     get(const PHLMONITOR);
        const std::vector<CMonitorRule>& all();
        std::vector<CMonitorRule>&       allMut();

      private:
        void                      performMonitorReload();

        std::vector<CMonitorRule> m_rules;
        bool                      m_reloadScheduled = false;

        struct {
            CHyprSignalListener preChecksRender;
        } m_listeners;
    };

    UP<CMonitorRuleManager>& monitorRuleMgr();
};