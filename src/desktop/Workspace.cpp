#include "Workspace.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "config/ConfigManager.hpp"
#include "managers/AnimationManager.hpp"
#include "../managers/EventManager.hpp"
#include "../managers/HookSystemManager.hpp"

#include <hyprutils/animation/AnimatedVariable.hpp>
#include <hyprutils/string/String.hpp>
using namespace Hyprutils::String;

PHLWORKSPACE CWorkspace::create(WORKSPACEID id, PHLMONITOR monitor, std::string name, bool special, bool isEmpty) {
    PHLWORKSPACE workspace = makeShared<CWorkspace>(id, monitor, name, special, isEmpty);
    workspace->init(workspace);
    g_pCompositor->registerWorkspace(workspace);
    return workspace;
}

CWorkspace::CWorkspace(WORKSPACEID id, PHLMONITOR monitor, std::string name, bool special, bool isEmpty) :
    m_id(id), m_name(name), m_monitor(monitor), m_isSpecialWorkspace(special), m_wasCreatedEmpty(isEmpty) {
    ;
}

void CWorkspace::init(PHLWORKSPACE self) {
    m_self = self;

    g_pAnimationManager->createAnimation(Vector2D(0, 0), m_renderOffset, g_pConfigManager->getAnimationPropertyConfig(m_isSpecialWorkspace ? "specialWorkspaceIn" : "workspacesIn"),
                                         self, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(1.f, m_alpha, g_pConfigManager->getAnimationPropertyConfig(m_isSpecialWorkspace ? "specialWorkspaceIn" : "workspacesIn"), self,
                                         AVARDAMAGE_ENTIRE);

    const auto RULEFORTHIS = g_pConfigManager->getWorkspaceRuleFor(self);
    if (RULEFORTHIS.defaultName.has_value())
        m_name = RULEFORTHIS.defaultName.value();

    m_focusedWindowHook = g_pHookSystem->hookDynamic("closeWindow", [this](void* self, SCallbackInfo& info, std::any param) {
        const auto PWINDOW = std::any_cast<PHLWINDOW>(param);

        if (PWINDOW == m_lastFocusedWindow.lock())
            m_lastFocusedWindow.reset();
    });

    m_inert = false;

    const auto WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(self);
    setPersistent(WORKSPACERULE.isPersistent);

    if (self->m_wasCreatedEmpty)
        if (auto cmd = WORKSPACERULE.onCreatedEmptyRunCmd)
            CKeybindManager::spawnWithRules(*cmd, self);

    g_pEventManager->postEvent({.event = "createworkspace", .data = m_name});
    g_pEventManager->postEvent({.event = "createworkspacev2", .data = std::format("{},{}", m_id, m_name)});
    EMIT_HOOK_EVENT("createWorkspace", this);
}

SWorkspaceIDName CWorkspace::getPrevWorkspaceIDName() const {
    return m_prevWorkspace;
}

CWorkspace::~CWorkspace() {
    Debug::log(LOG, "Destroying workspace ID {}", m_id);

    // check if g_pHookSystem and g_pEventManager exist, they might be destroyed as in when the compositor is closing.
    if (g_pHookSystem)
        g_pHookSystem->unhook(m_focusedWindowHook);

    if (g_pEventManager) {
        g_pEventManager->postEvent({.event = "destroyworkspace", .data = m_name});
        g_pEventManager->postEvent({.event = "destroyworkspacev2", .data = std::format("{},{}", m_id, m_name)});
        EMIT_HOOK_EVENT("destroyWorkspace", this);
    }

    m_events.destroy.emit();
}

void CWorkspace::startAnim(bool in, bool left, bool instant) {
    if (!instant) {
        const std::string ANIMNAME = std::format("{}{}", m_isSpecialWorkspace ? "specialWorkspace" : "workspaces", in ? "In" : "Out");

        m_alpha->setConfig(g_pConfigManager->getAnimationPropertyConfig(ANIMNAME));
        m_renderOffset->setConfig(g_pConfigManager->getAnimationPropertyConfig(ANIMNAME));
    }

    const auto  ANIMSTYLE     = m_alpha->getStyle();
    static auto PWORKSPACEGAP = CConfigValue<Hyprlang::INT>("general:gaps_workspaces");

    // set floating windows offset callbacks
    m_renderOffset->setUpdateCallback([&](auto) {
        for (auto const& w : g_pCompositor->m_windows) {
            if (!validMapped(w) || w->workspaceID() != m_id)
                continue;

            w->onWorkspaceAnimUpdate();
        };
    });

    if (ANIMSTYLE.starts_with("slidefade")) {
        const auto PMONITOR = m_monitor.lock();
        float      movePerc = 100.f;

        if (ANIMSTYLE.find('%') != std::string::npos) {
            try {
                auto percstr = ANIMSTYLE.substr(ANIMSTYLE.find_last_of(' ') + 1);
                movePerc     = std::stoi(percstr.substr(0, percstr.length() - 1));
            } catch (std::exception& e) { Debug::log(ERR, "Error in startAnim: invalid percentage"); }
        }

        m_alpha->setValueAndWarp(1.f);
        m_renderOffset->setValueAndWarp(Vector2D(0, 0));

        if (ANIMSTYLE.starts_with("slidefadevert")) {
            if (in) {
                m_alpha->setValueAndWarp(0.f);
                m_renderOffset->setValueAndWarp(Vector2D(0.0, (left ? PMONITOR->m_size.y : -PMONITOR->m_size.y) * (movePerc / 100.f)));
                *m_alpha        = 1.f;
                *m_renderOffset = Vector2D(0, 0);
            } else {
                m_alpha->setValueAndWarp(1.f);
                *m_alpha        = 0.f;
                *m_renderOffset = Vector2D(0.0, (left ? -PMONITOR->m_size.y : PMONITOR->m_size.y) * (movePerc / 100.f));
            }
        } else {
            if (in) {
                m_alpha->setValueAndWarp(0.f);
                m_renderOffset->setValueAndWarp(Vector2D((left ? PMONITOR->m_size.x : -PMONITOR->m_size.x) * (movePerc / 100.f), 0.0));
                *m_alpha        = 1.f;
                *m_renderOffset = Vector2D(0, 0);
            } else {
                m_alpha->setValueAndWarp(1.f);
                *m_alpha        = 0.f;
                *m_renderOffset = Vector2D((left ? -PMONITOR->m_size.x : PMONITOR->m_size.x) * (movePerc / 100.f), 0.0);
            }
        }
    } else if (ANIMSTYLE == "fade") {
        m_renderOffset->setValueAndWarp(Vector2D(0, 0)); // fix a bug, if switching from slide -> fade.

        if (in) {
            m_alpha->setValueAndWarp(0.f);
            *m_alpha = 1.f;
        } else {
            m_alpha->setValueAndWarp(1.f);
            *m_alpha = 0.f;
        }
    } else if (ANIMSTYLE == "slidevert") {
        // fallback is slide
        const auto PMONITOR  = m_monitor.lock();
        const auto YDISTANCE = PMONITOR->m_size.y + *PWORKSPACEGAP;

        m_alpha->setValueAndWarp(1.f); // fix a bug, if switching from fade -> slide.

        if (in) {
            m_renderOffset->setValueAndWarp(Vector2D(0.0, left ? YDISTANCE : -YDISTANCE));
            *m_renderOffset = Vector2D(0, 0);
        } else {
            *m_renderOffset = Vector2D(0.0, left ? -YDISTANCE : YDISTANCE);
        }
    } else {
        // fallback is slide
        const auto PMONITOR  = m_monitor.lock();
        const auto XDISTANCE = PMONITOR->m_size.x + *PWORKSPACEGAP;

        m_alpha->setValueAndWarp(1.f); // fix a bug, if switching from fade -> slide.

        if (in) {
            m_renderOffset->setValueAndWarp(Vector2D(left ? XDISTANCE : -XDISTANCE, 0.0));
            *m_renderOffset = Vector2D(0, 0);
        } else {
            *m_renderOffset = Vector2D(left ? -XDISTANCE : XDISTANCE, 0.0);
        }
    }

    if (m_isSpecialWorkspace) {
        // required for open/close animations
        if (in) {
            m_alpha->setValueAndWarp(0.f);
            *m_alpha = 1.f;
        } else {
            m_alpha->setValueAndWarp(1.f);
            *m_alpha = 0.f;
        }
    }

    if (instant) {
        m_renderOffset->warp();
        m_alpha->warp();
    }
}

PHLWINDOW CWorkspace::getLastFocusedWindow() {
    if (!validMapped(m_lastFocusedWindow) || m_lastFocusedWindow->workspaceID() != m_id)
        return nullptr;

    return m_lastFocusedWindow.lock();
}

void CWorkspace::rememberPrevWorkspace(const PHLWORKSPACE& prev) {
    if (!prev) {
        m_prevWorkspace.id   = -1;
        m_prevWorkspace.name = "";
        return;
    }

    if (prev->m_id == m_id) {
        Debug::log(LOG, "Tried to set prev workspace to the same as current one");
        return;
    }

    m_prevWorkspace.id   = prev->m_id;
    m_prevWorkspace.name = prev->m_name;

    prev->m_monitor->addPrevWorkspaceID(prev->m_id);
}

std::string CWorkspace::getConfigName() {
    if (m_isSpecialWorkspace) {
        return m_name;
    }

    if (m_id > 0)
        return std::to_string(m_id);

    return "name:" + m_name;
}

bool CWorkspace::matchesStaticSelector(const std::string& selector_) {
    auto selector = trim(selector_);

    if (selector.empty())
        return true;

    if (isNumber(selector)) {
        const auto& [wsid, wsname] = getWorkspaceIDNameFromString(selector);

        if (wsid == WORKSPACE_INVALID)
            return false;

        return wsid == m_id;

    } else if (selector.starts_with("name:")) {
        return m_name == selector.substr(5);
    } else if (selector.starts_with("special")) {
        return m_name == selector;
    } else {
        // parse selector

        for (size_t i = 0; i < selector.length(); ++i) {
            const char& cur = selector[i];
            if (std::isspace(cur))
                continue;

            // Allowed selectors:
            // r - range: r[1-5]
            // s - special: s[true]
            // n - named: n[true] or n[s:string] or n[e:string]
            // m - monitor: m[monitor_selector]
            // w - windowCount: w[1-4] or w[1], optional flag t or f for tiled or floating and
            //                  flag p to count only pinned windows, e.g. w[p1-2], w[pg4]
            //                  flag g to count groups instead of windows, e.g. w[t1-2], w[fg4]
            //                  flag v will count only visible windows
            // f - fullscreen state : f[-1], f[0], f[1], or f[2] for different fullscreen states
            //                        -1: no fullscreen, 0: fullscreen, 1: maximized, 2: fullscreen without sending fs state to window

            const auto  CLOSING_BRACKET = selector.find_first_of(']', i);
            std::string prop            = selector.substr(i, CLOSING_BRACKET == std::string::npos ? std::string::npos : CLOSING_BRACKET + 1 - i);
            i                           = std::min(CLOSING_BRACKET, std::string::npos - 1);

            if (cur == 'r') {
                WORKSPACEID from = 0, to = 0;
                if (!prop.starts_with("r[") || !prop.ends_with("]")) {
                    Debug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                prop = prop.substr(2, prop.length() - 3);

                if (!prop.contains("-")) {
                    Debug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                const auto DASHPOS = prop.find('-');
                const auto LHS = prop.substr(0, DASHPOS), RHS = prop.substr(DASHPOS + 1);

                if (!isNumber(LHS) || !isNumber(RHS)) {
                    Debug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                try {
                    from = std::stoll(LHS);
                    to   = std::stoll(RHS);
                } catch (std::exception& e) {
                    Debug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                if (to < from || to < 1 || from < 1) {
                    Debug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                if (std::clamp(m_id, from, to) != m_id)
                    return false;
                continue;
            }

            if (cur == 's') {
                if (!prop.starts_with("s[") || !prop.ends_with("]")) {
                    Debug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                prop = prop.substr(2, prop.length() - 3);

                const auto SHOULDBESPECIAL = configStringToInt(prop);

                if (SHOULDBESPECIAL && sc<bool>(*SHOULDBESPECIAL) != m_isSpecialWorkspace)
                    return false;
                continue;
            }

            if (cur == 'm') {
                if (!prop.starts_with("m[") || !prop.ends_with("]")) {
                    Debug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                prop = prop.substr(2, prop.length() - 3);

                const auto PMONITOR = g_pCompositor->getMonitorFromString(prop);

                if (!(PMONITOR ? PMONITOR == m_monitor : false))
                    return false;
                continue;
            }

            if (cur == 'n') {
                if (!prop.starts_with("n[") || !prop.ends_with("]")) {
                    Debug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                prop = prop.substr(2, prop.length() - 3);

                if (prop.starts_with("s:") && !m_name.starts_with(prop.substr(2)))
                    return false;
                if (prop.starts_with("e:") && !m_name.ends_with(prop.substr(2)))
                    return false;

                const auto WANTSNAMED = configStringToInt(prop);

                if (WANTSNAMED && *WANTSNAMED != (m_id <= -1337))
                    return false;
                continue;
            }

            if (cur == 'w') {
                WORKSPACEID from = 0, to = 0;
                if (!prop.starts_with("w[") || !prop.ends_with("]")) {
                    Debug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                prop = prop.substr(2, prop.length() - 3);

                int  wantsOnlyTiled    = -1;
                int  wantsOnlyPinned   = false;
                bool wantsCountGroup   = false;
                bool wantsCountVisible = false;

                int  flagCount = 0;
                for (auto const& flag : prop) {
                    if (flag == 't' && wantsOnlyTiled == -1) {
                        wantsOnlyTiled = 1;
                        flagCount++;
                    } else if (flag == 'f' && wantsOnlyTiled == -1) {
                        wantsOnlyTiled = 0;
                        flagCount++;
                    } else if (flag == 'p' && !wantsOnlyPinned) {
                        wantsOnlyPinned = true;
                        flagCount++;
                    } else if (flag == 'g' && !wantsCountGroup) {
                        wantsCountGroup = true;
                        flagCount++;
                    } else if (flag == 'v' && !wantsCountVisible) {
                        wantsCountVisible = true;
                        flagCount++;
                    } else {
                        break;
                    }
                }
                prop = prop.substr(flagCount);

                if (!prop.contains("-")) {
                    // try single

                    if (!isNumber(prop)) {
                        Debug::log(LOG, "Invalid selector {}", selector);
                        return false;
                    }

                    try {
                        from = std::stoll(prop);
                    } catch (std::exception& e) {
                        Debug::log(LOG, "Invalid selector {}", selector);
                        return false;
                    }

                    int count;
                    if (wantsCountGroup)
                        count = getGroups(wantsOnlyTiled == -1 ? std::nullopt : std::optional<bool>(sc<bool>(wantsOnlyTiled)),
                                          wantsOnlyPinned ? std::optional<bool>(wantsOnlyPinned) : std::nullopt,
                                          wantsCountVisible ? std::optional<bool>(wantsCountVisible) : std::nullopt);
                    else
                        count = getWindows(wantsOnlyTiled == -1 ? std::nullopt : std::optional<bool>(sc<bool>(wantsOnlyTiled)),
                                           wantsOnlyPinned ? std::optional<bool>(wantsOnlyPinned) : std::nullopt,
                                           wantsCountVisible ? std::optional<bool>(wantsCountVisible) : std::nullopt);

                    if (count != from)
                        return false;
                    continue;
                }

                const auto DASHPOS = prop.find('-');
                const auto LHS = prop.substr(0, DASHPOS), RHS = prop.substr(DASHPOS + 1);

                if (!isNumber(LHS) || !isNumber(RHS)) {
                    Debug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                try {
                    from = std::stoll(LHS);
                    to   = std::stoll(RHS);
                } catch (std::exception& e) {
                    Debug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                if (to < from || to < 1 || from < 1) {
                    Debug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                WORKSPACEID count;
                if (wantsCountGroup)
                    count =
                        getGroups(wantsOnlyTiled == -1 ? std::nullopt : std::optional<bool>(sc<bool>(wantsOnlyTiled)),
                                  wantsOnlyPinned ? std::optional<bool>(wantsOnlyPinned) : std::nullopt, wantsCountVisible ? std::optional<bool>(wantsCountVisible) : std::nullopt);
                else
                    count = getWindows(wantsOnlyTiled == -1 ? std::nullopt : std::optional<bool>(sc<bool>(wantsOnlyTiled)),
                                       wantsOnlyPinned ? std::optional<bool>(wantsOnlyPinned) : std::nullopt,
                                       wantsCountVisible ? std::optional<bool>(wantsCountVisible) : std::nullopt);

                if (std::clamp(count, from, to) != count)
                    return false;
                continue;
            }

            if (cur == 'f') {
                if (!prop.starts_with("f[") || !prop.ends_with("]")) {
                    Debug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                prop        = prop.substr(2, prop.length() - 3);
                int FSSTATE = -1;
                try {
                    FSSTATE = std::stoi(prop);
                } catch (std::exception& e) {
                    Debug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                switch (FSSTATE) {
                    case -1: // no fullscreen
                        if (m_hasFullscreenWindow)
                            return false;
                        break;
                    case 0: // fullscreen full
                        if (!m_hasFullscreenWindow || m_fullscreenMode != FSMODE_FULLSCREEN)
                            return false;
                        break;
                    case 1: // maximized
                        if (!m_hasFullscreenWindow || m_fullscreenMode != FSMODE_MAXIMIZED)
                            return false;
                        break;
                    default: break;
                }
                continue;
            }

            Debug::log(LOG, "Invalid selector {}", selector);
            return false;
        }

        return true;
    }

    UNREACHABLE();
    return false;
}

void CWorkspace::markInert() {
    m_inert   = true;
    m_id      = WORKSPACE_INVALID;
    m_visible = false;
    m_monitor.reset();
}

bool CWorkspace::inert() {
    return m_inert;
}

MONITORID CWorkspace::monitorID() {
    return m_monitor ? m_monitor->m_id : MONITOR_INVALID;
}

PHLWINDOW CWorkspace::getFullscreenWindow() {
    for (auto const& w : g_pCompositor->m_windows) {
        if (w->m_workspace == m_self && w->isFullscreen())
            return w;
    }

    return nullptr;
}

bool CWorkspace::isVisible() {
    return m_visible;
}

bool CWorkspace::isVisibleNotCovered() {
    const auto PMONITOR = m_monitor.lock();
    if (PMONITOR->m_activeSpecialWorkspace)
        return PMONITOR->m_activeSpecialWorkspace->m_id == m_id;

    return PMONITOR->m_activeWorkspace->m_id == m_id;
}

int CWorkspace::getWindows(std::optional<bool> onlyTiled, std::optional<bool> onlyPinned, std::optional<bool> onlyVisible) {
    int no = 0;
    for (auto const& w : g_pCompositor->m_windows) {
        if (w->workspaceID() != m_id || !w->m_isMapped)
            continue;
        if (onlyTiled.has_value() && w->m_isFloating == onlyTiled.value())
            continue;
        if (onlyPinned.has_value() && w->m_pinned != onlyPinned.value())
            continue;
        if (onlyVisible.has_value() && w->isHidden() == onlyVisible.value())
            continue;
        no++;
    }

    return no;
}

int CWorkspace::getGroups(std::optional<bool> onlyTiled, std::optional<bool> onlyPinned, std::optional<bool> onlyVisible) {
    int no = 0;
    for (auto const& w : g_pCompositor->m_windows) {
        if (w->workspaceID() != m_id || !w->m_isMapped)
            continue;
        if (!w->m_groupData.head)
            continue;
        if (onlyTiled.has_value() && w->m_isFloating == onlyTiled.value())
            continue;
        if (onlyPinned.has_value() && w->m_pinned != onlyPinned.value())
            continue;
        if (onlyVisible.has_value() && w->isHidden() == onlyVisible.value())
            continue;
        no++;
    }
    return no;
}

PHLWINDOW CWorkspace::getFirstWindow() {
    for (auto const& w : g_pCompositor->m_windows) {
        if (w->m_workspace == m_self && w->m_isMapped && !w->isHidden())
            return w;
    }

    return nullptr;
}

PHLWINDOW CWorkspace::getTopLeftWindow() {
    const auto PMONITOR = m_monitor.lock();

    for (auto const& w : g_pCompositor->m_windows) {
        if (w->m_workspace != m_self || !w->m_isMapped || w->isHidden())
            continue;

        const auto WINDOWIDEALBB = w->getWindowIdealBoundingBoxIgnoreReserved();

        if (WINDOWIDEALBB.x <= PMONITOR->m_position.x + 1 && WINDOWIDEALBB.y <= PMONITOR->m_position.y + 1)
            return w;
    }
    return nullptr;
}

bool CWorkspace::hasUrgentWindow() {
    return std::ranges::any_of(g_pCompositor->m_windows, [this](const auto& w) { return w->m_workspace == m_self && w->m_isMapped && w->m_isUrgent; });
}

void CWorkspace::updateWindowDecos() {
    for (auto const& w : g_pCompositor->m_windows) {
        if (w->m_workspace != m_self)
            continue;

        w->updateWindowDecos();
    }
}

void CWorkspace::updateWindowData() {
    const auto WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(m_self.lock());

    for (auto const& w : g_pCompositor->m_windows) {
        if (w->m_workspace != m_self)
            continue;

        w->updateWindowData(WORKSPACERULE);
    }
}

void CWorkspace::forceReportSizesToWindows() {
    for (auto const& w : g_pCompositor->m_windows) {
        if (w->m_workspace != m_self || !w->m_isMapped || w->isHidden())
            continue;

        w->sendWindowSize(true);
    }
}

void CWorkspace::rename(const std::string& name) {
    if (g_pCompositor->isWorkspaceSpecial(m_id))
        return;

    Debug::log(LOG, "CWorkspace::rename: Renaming workspace {} to '{}'", m_id, name);
    m_name = name;

    const auto WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(m_self.lock());
    setPersistent(WORKSPACERULE.isPersistent);

    if (WORKSPACERULE.isPersistent)
        g_pCompositor->ensurePersistentWorkspacesPresent(std::vector<SWorkspaceRule>{WORKSPACERULE}, m_self.lock());

    g_pEventManager->postEvent({.event = "renameworkspace", .data = std::to_string(m_id) + "," + m_name});
    m_events.renamed.emit();
}

void CWorkspace::updateWindows() {
    m_hasFullscreenWindow = std::ranges::any_of(g_pCompositor->m_windows, [this](const auto& w) { return w->m_isMapped && w->m_workspace == m_self && w->isFullscreen(); });

    for (auto const& w : g_pCompositor->m_windows) {
        if (!w->m_isMapped || w->m_workspace != m_self)
            continue;

        w->updateDynamicRules();
    }
}

void CWorkspace::setPersistent(bool persistent) {
    if (m_persistent == persistent)
        return;

    m_persistent = persistent;

    if (persistent)
        m_selfPersistent = m_self.lock();
    else
        m_selfPersistent.reset();
}

bool CWorkspace::isPersistent() {
    return m_persistent;
}
