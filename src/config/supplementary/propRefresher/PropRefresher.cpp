#include "PropRefresher.hpp"

#include "../../../managers/eventLoop/EventLoopManager.hpp"
#include "../../../managers/input/InputManager.hpp"
#include "../../../render/Renderer.hpp"
#include "../../../render/decorations/CHyprGroupBarDecoration.hpp"
#include "../../../Compositor.hpp"
#include "../../../layout/supplementary/WorkspaceAlgoMatcher.hpp"
#include "../../../layout/LayoutManager.hpp"
#include "../../../layout/space/Space.hpp"
#include "../../../desktop/rule/Engine.hpp"
#include "../../../desktop/state/GlobalWindowController.hpp"
#include "../../../state/MonitorState.hpp"
#include "../../../state/WorkspacePlacementController.hpp"
#include "../../../state/WorkspaceState.hpp"

#include "../../shared/monitor/MonitorRuleManager.hpp"
#include "../../shared/inotify/ConfigWatcher.hpp"
#include "../../../event/EventBus.hpp"

using namespace Config;
using namespace Config::Supplementary;

UP<CPropRefresher>& Supplementary::refresher() {
    static UP<CPropRefresher> p = makeUnique<CPropRefresher>();
    return p;
}

void CPropRefresher::scheduleRefresh(PropRefreshBits prop) {

    m_propsTripped |= prop;

    if (!m_scheduled && g_pEventLoopManager) {
        m_scheduledRefreshSeq = g_pEventLoopManager->doLater([this, weak = WP<CPropRefresher>{refresher()}] {
            if (!weak)
                return;
            refreshProp(true);
        });

        m_scheduled = true;
    }
}

int CPropRefresher::executeScheduledRefreshImmediately() {

    if (!m_scheduled || m_scheduledRefreshSeq == 0)
        return 1;

    g_pEventLoopManager->removeDoLater(m_scheduledRefreshSeq);
    // m_scheduledRefreshSeq must be reset back to 0 during refreshProp() call
    refreshProp(false);
    return 0;
}

void CPropRefresher::refreshProp(const bool execdAsScheduled) {

    static auto PZOOMFACTOR = CConfigValue<Config::FLOAT>("cursor:zoom_factor");

    if (m_propsTripped & REFRESH_INPUT_DEVICES) {
        g_pInputManager->setKeyboardLayout();     // update kb layout
        g_pInputManager->setPointerConfigs();     // update mouse cfgs
        g_pInputManager->setTouchDeviceConfigs(); // update touch device cfgs
        g_pInputManager->setTabletConfigs();      // update tablets
        g_pInputManager->setTabletToolConfigs();  // update tablettools
    }

    if (m_propsTripped & REFRESH_SCREEN_SHADER) {
        g_pHyprRenderer->m_reloadScreenShader = true;
        for (auto const& m : State::monitorState()->monitors()) {
            if (!m)
                continue;

            m->m_forceFullFrames = 2;
            m->scheduleFrame();
        }
    }

    if (m_propsTripped & REFRESH_BLUR_FB) {
        for (auto const& m : State::monitorState()->monitors()) {
            if (!m)
                continue;

            m->m_blurFBDirty     = true;
            m->m_forceFullFrames = 2;
            m->scheduleFrame();
        }
    }

    if (m_propsTripped & REFRESH_WINDOW_STATES) {
        Desktop::Rule::ruleEngine()->updateAllRules();

        for (const auto& ws : State::workspaceState()->workspaces()) {
            if (!ws)
                continue;

            ws->updateWindows();
            ws->updateWindowData();
            ws->updateWindowDecos();
        }

        Desktop::globalWindowController()->updateAllWindowsDecorations();

        for (auto const& m : State::monitorState()->monitors()) {
            if (!m)
                continue;

            m->m_forceFullFrames = 2;
            g_pHyprRenderer->damageMonitor(m);
            m->scheduleFrame();
        }
    }

    if (m_propsTripped & REFRESH_MONITOR_STATES) {
        Config::monitorRuleMgr()->scheduleReload();
        Config::monitorRuleMgr()->ensureVRR();

        for (const auto& m : State::monitorState()->monitors()) {
            if (!m)
                continue;

            g_layoutManager->recalculateMonitor(m);
        }

        State::workspacePlacementController()->ensurePersistentWorkspacesPresent(
            nullptr, [](PHLWORKSPACE ws, PHLMONITOR mon, bool noWarp) { State::workspacePlacementController()->moveWorkspaceToMonitor(ws, mon, noWarp); });
    }

    if (m_propsTripped & REFRESH_LAYOUTS) {
        Layout::Supplementary::algoMatcher()->updateWorkspaceLayouts();

        for (auto const& m : State::monitorState()->monitors()) {
            g_layoutManager->recalculateMonitor(m);
            g_pHyprRenderer->damageMonitor(m);
        }
    }

    if (m_propsTripped & REFRESH_CURSOR_ZOOMS) {
        for (auto const& m : State::monitorState()->monitors()) {
            *(m->m_cursorZoom) = *PZOOMFACTOR;
            if (m->m_activeWorkspace)
                m->m_activeWorkspace->m_space->recalculate();
        }
    }

    if (m_propsTripped & REFRESH_CONFIG_WATCHER)
        Config::watcher()->update();

    if (m_propsTripped & REFRESH_GRADIENTS_GROUPBAR)
        refreshGroupBarGradients();

    m_scheduled           = false;
    m_scheduledRefreshSeq = 0;
    m_propsTripped        = 0;

    Event::bus()->m_events.config.props_refreshed.emit(execdAsScheduled);
}
