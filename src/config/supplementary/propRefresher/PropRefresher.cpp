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

#include "../../shared/monitor/MonitorRuleManager.hpp"
#include "../../shared/inotify/ConfigWatcher.hpp"

using namespace Config;
using namespace Config::Supplementary;

UP<CPropRefresher>& Supplementary::refresher() {
    static UP<CPropRefresher> p = makeUnique<CPropRefresher>();
    return p;
}

void CPropRefresher::scheduleRefresh(PropRefreshBits prop) {
    static auto PZOOMFACTOR = CConfigValue<Config::FLOAT>("cursor:zoom_factor");

    m_propsTripped |= prop;

    if (!m_scheduled && g_pEventLoopManager) {
        g_pEventLoopManager->doLater([this, weak = WP<CPropRefresher>{refresher()}] {
            if (!weak)
                return;

            if (m_propsTripped & REFRESH_INPUT_DEVICES) {
                g_pInputManager->setKeyboardLayout();     // update kb layout
                g_pInputManager->setPointerConfigs();     // update mouse cfgs
                g_pInputManager->setTouchDeviceConfigs(); // update touch device cfgs
                g_pInputManager->setTabletConfigs();      // update tablets
            }

            if (m_propsTripped & REFRESH_SCREEN_SHADER)
                g_pHyprRenderer->m_reloadScreenShader = true;

            if (m_propsTripped & REFRESH_BLUR_FB) {
                for (auto const& m : g_pCompositor->m_monitors) {
                    if (m)
                        m->m_blurFBDirty = true;
                }
            }

            if (m_propsTripped & REFRESH_WINDOW_STATES) {
                Desktop::Rule::ruleEngine()->updateAllRules();

                for (const auto& ws : g_pCompositor->getWorkspaces()) {
                    if (!ws)
                        continue;

                    ws->updateWindows();
                    ws->updateWindowData();
                    ws->updateWindowDecos();
                }

                g_pCompositor->updateAllWindowsAnimatedDecorationValues();
            }

            if (m_propsTripped & REFRESH_MONITOR_STATES) {
                Config::monitorRuleMgr()->scheduleReload();
                Config::monitorRuleMgr()->ensureVRR();

                for (const auto& m : g_pCompositor->m_monitors) {
                    if (!m)
                        continue;

                    g_layoutManager->recalculateMonitor(m);
                }

                g_pCompositor->ensurePersistentWorkspacesPresent();
            }

            if (m_propsTripped & REFRESH_LAYOUTS) {
                Layout::Supplementary::algoMatcher()->updateWorkspaceLayouts();

                for (auto const& m : g_pCompositor->m_monitors) {
                    g_layoutManager->recalculateMonitor(m);
                    g_pHyprRenderer->damageMonitor(m);
                }
            }

            if (m_propsTripped & REFRESH_CURSOR_ZOOMS) {
                for (auto const& m : g_pCompositor->m_monitors) {
                    *(m->m_cursorZoom) = *PZOOMFACTOR;
                    if (m->m_activeWorkspace)
                        m->m_activeWorkspace->m_space->recalculate();
                }
            }

            if (m_propsTripped & REFRESH_CONFIG_WATCHER)
                Config::watcher()->update();

            if (m_propsTripped & REFRESH_GRADIENTS_GROUPBAR)
                refreshGroupBarGradients();

            m_scheduled    = false;
            m_propsTripped = 0;
        });

        m_scheduled = true;
    }
}