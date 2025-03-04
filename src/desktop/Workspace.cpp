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
    return workspace;
}

CWorkspace::CWorkspace(WORKSPACEID id, PHLMONITOR monitor, std::string name, bool special, bool isEmpty) :
    m_iID(id), m_szName(name), m_pMonitor(monitor), m_bIsSpecialWorkspace(special), m_bWasCreatedEmpty(isEmpty) {
    ;
}

void CWorkspace::init(PHLWORKSPACE self) {
    m_pSelf = self;

    g_pAnimationManager->createAnimation(Vector2D(0, 0), m_vRenderOffset,
                                         g_pConfigManager->getAnimationPropertyConfig(m_bIsSpecialWorkspace ? "specialWorkspaceIn" : "workspacesIn"), self, AVARDAMAGE_ENTIRE);
    g_pAnimationManager->createAnimation(1.f, m_fAlpha, g_pConfigManager->getAnimationPropertyConfig(m_bIsSpecialWorkspace ? "specialWorkspaceIn" : "workspacesIn"), self,
                                         AVARDAMAGE_ENTIRE);

    const auto RULEFORTHIS = g_pConfigManager->getWorkspaceRuleFor(self);
    if (RULEFORTHIS.defaultName.has_value())
        m_szName = RULEFORTHIS.defaultName.value();

    m_pFocusedWindowHook = g_pHookSystem->hookDynamic("closeWindow", [this](void* self, SCallbackInfo& info, std::any param) {
        const auto PWINDOW = std::any_cast<PHLWINDOW>(param);

        if (PWINDOW == m_pLastFocusedWindow.lock())
            m_pLastFocusedWindow.reset();
    });

    m_bInert = false;

    const auto WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(self);
    m_bPersistent            = WORKSPACERULE.isPersistent;

    if (self->m_bWasCreatedEmpty)
        if (auto cmd = WORKSPACERULE.onCreatedEmptyRunCmd)
            g_pKeybindManager->spawnWithRules(*cmd, self);

    g_pEventManager->postEvent({"createworkspace", m_szName});
    g_pEventManager->postEvent({"createworkspacev2", std::format("{},{}", m_iID, m_szName)});
    EMIT_HOOK_EVENT("createWorkspace", this);
}

SWorkspaceIDName CWorkspace::getPrevWorkspaceIDName() const {
    return m_sPrevWorkspace;
}

CWorkspace::~CWorkspace() {
    NDebug::log(LOG, "Destroying workspace ID {}", m_iID);

    // check if g_pHookSystem and g_pEventManager exist, they might be destroyed as in when the compositor is closing.
    if (g_pHookSystem)
        g_pHookSystem->unhook(m_pFocusedWindowHook);

    if (g_pEventManager) {
        g_pEventManager->postEvent({"destroyworkspace", m_szName});
        g_pEventManager->postEvent({"destroyworkspacev2", std::format("{},{}", m_iID, m_szName)});
        EMIT_HOOK_EVENT("destroyWorkspace", this);
    }
}

void CWorkspace::startAnim(bool in, bool left, bool instant) {
    if (!instant) {
        const std::string ANIMNAME = std::format("{}{}", m_bIsSpecialWorkspace ? "specialWorkspace" : "workspaces", in ? "In" : "Out");

        m_fAlpha->setConfig(g_pConfigManager->getAnimationPropertyConfig(ANIMNAME));
        m_vRenderOffset->setConfig(g_pConfigManager->getAnimationPropertyConfig(ANIMNAME));
    }

    const auto  ANIMSTYLE     = m_fAlpha->getStyle();
    static auto PWORKSPACEGAP = CConfigValue<Hyprlang::INT>("general:gaps_workspaces");

    // set floating windows offset callbacks
    m_vRenderOffset->setUpdateCallback([&](auto) {
        for (auto const& w : g_pCompositor->m_vWindows) {
            if (!validMapped(w) || w->workspaceID() != m_iID)
                continue;

            w->onWorkspaceAnimUpdate();
        };
    });

    if (ANIMSTYLE.starts_with("slidefade")) {
        const auto PMONITOR = m_pMonitor.lock();
        float      movePerc = 100.f;

        if (ANIMSTYLE.find('%') != std::string::npos) {
            try {
                auto percstr = ANIMSTYLE.substr(ANIMSTYLE.find_last_of(' ') + 1);
                movePerc     = std::stoi(percstr.substr(0, percstr.length() - 1));
            } catch (std::exception& e) { NDebug::log(ERR, "Error in startAnim: invalid percentage"); }
        }

        m_fAlpha->setValueAndWarp(1.f);
        m_vRenderOffset->setValueAndWarp(Vector2D(0, 0));

        if (ANIMSTYLE.starts_with("slidefadevert")) {
            if (in) {
                m_fAlpha->setValueAndWarp(0.f);
                m_vRenderOffset->setValueAndWarp(Vector2D(0.0, (left ? PMONITOR->vecSize.y : -PMONITOR->vecSize.y) * (movePerc / 100.f)));
                *m_fAlpha        = 1.f;
                *m_vRenderOffset = Vector2D(0, 0);
            } else {
                m_fAlpha->setValueAndWarp(1.f);
                *m_fAlpha        = 0.f;
                *m_vRenderOffset = Vector2D(0.0, (left ? -PMONITOR->vecSize.y : PMONITOR->vecSize.y) * (movePerc / 100.f));
            }
        } else {
            if (in) {
                m_fAlpha->setValueAndWarp(0.f);
                m_vRenderOffset->setValueAndWarp(Vector2D((left ? PMONITOR->vecSize.x : -PMONITOR->vecSize.x) * (movePerc / 100.f), 0.0));
                *m_fAlpha        = 1.f;
                *m_vRenderOffset = Vector2D(0, 0);
            } else {
                m_fAlpha->setValueAndWarp(1.f);
                *m_fAlpha        = 0.f;
                *m_vRenderOffset = Vector2D((left ? -PMONITOR->vecSize.x : PMONITOR->vecSize.x) * (movePerc / 100.f), 0.0);
            }
        }
    } else if (ANIMSTYLE == "fade") {
        m_vRenderOffset->setValueAndWarp(Vector2D(0, 0)); // fix a bug, if switching from slide -> fade.

        if (in) {
            m_fAlpha->setValueAndWarp(0.f);
            *m_fAlpha = 1.f;
        } else {
            m_fAlpha->setValueAndWarp(1.f);
            *m_fAlpha = 0.f;
        }
    } else if (ANIMSTYLE == "slidevert") {
        // fallback is slide
        const auto PMONITOR  = m_pMonitor.lock();
        const auto YDISTANCE = PMONITOR->vecSize.y + *PWORKSPACEGAP;

        m_fAlpha->setValueAndWarp(1.f); // fix a bug, if switching from fade -> slide.

        if (in) {
            m_vRenderOffset->setValueAndWarp(Vector2D(0.0, left ? YDISTANCE : -YDISTANCE));
            *m_vRenderOffset = Vector2D(0, 0);
        } else {
            *m_vRenderOffset = Vector2D(0.0, left ? -YDISTANCE : YDISTANCE);
        }
    } else {
        // fallback is slide
        const auto PMONITOR  = m_pMonitor.lock();
        const auto XDISTANCE = PMONITOR->vecSize.x + *PWORKSPACEGAP;

        m_fAlpha->setValueAndWarp(1.f); // fix a bug, if switching from fade -> slide.

        if (in) {
            m_vRenderOffset->setValueAndWarp(Vector2D(left ? XDISTANCE : -XDISTANCE, 0.0));
            *m_vRenderOffset = Vector2D(0, 0);
        } else {
            *m_vRenderOffset = Vector2D(left ? -XDISTANCE : XDISTANCE, 0.0);
        }
    }

    if (m_bIsSpecialWorkspace) {
        // required for open/close animations
        if (in) {
            m_fAlpha->setValueAndWarp(0.f);
            *m_fAlpha = 1.f;
        } else {
            m_fAlpha->setValueAndWarp(1.f);
            *m_fAlpha = 0.f;
        }
    }

    if (instant) {
        m_vRenderOffset->warp();
        m_fAlpha->warp();
    }
}

void CWorkspace::setActive(bool on) {
    ; // empty until https://gitlab.freedesktop.org/wayland/wayland-protocols/-/merge_requests/40
}

void CWorkspace::moveToMonitor(const MONITORID& id) {
    ; // empty until https://gitlab.freedesktop.org/wayland/wayland-protocols/-/merge_requests/40
}

PHLWINDOW CWorkspace::getLastFocusedWindow() {
    if (!validMapped(m_pLastFocusedWindow) || m_pLastFocusedWindow->workspaceID() != m_iID)
        return nullptr;

    return m_pLastFocusedWindow.lock();
}

void CWorkspace::rememberPrevWorkspace(const PHLWORKSPACE& prev) {
    if (!prev) {
        m_sPrevWorkspace.id   = -1;
        m_sPrevWorkspace.name = "";
        return;
    }

    if (prev->m_iID == m_iID) {
        NDebug::log(LOG, "Tried to set prev workspace to the same as current one");
        return;
    }

    m_sPrevWorkspace.id   = prev->m_iID;
    m_sPrevWorkspace.name = prev->m_szName;

    prev->m_pMonitor->addPrevWorkspaceID(prev->m_iID);
}

std::string CWorkspace::getConfigName() {
    if (m_bIsSpecialWorkspace) {
        return m_szName;
    }

    if (m_iID > 0)
        return std::to_string(m_iID);

    return "name:" + m_szName;
}

bool CWorkspace::matchesStaticSelector(const std::string& selector_) {
    auto selector = trim(selector_);

    if (selector.empty())
        return true;

    if (isNumber(selector)) {
        const auto& [wsid, wsname] = getWorkspaceIDNameFromString(selector);

        if (wsid == WORKSPACE_INVALID)
            return false;

        return wsid == m_iID;

    } else if (selector.starts_with("name:")) {
        return m_szName == selector.substr(5);
    } else if (selector.starts_with("special")) {
        return m_szName == selector;
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
                    NDebug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                prop = prop.substr(2, prop.length() - 3);

                if (!prop.contains("-")) {
                    NDebug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                const auto DASHPOS = prop.find('-');
                const auto LHS = prop.substr(0, DASHPOS), RHS = prop.substr(DASHPOS + 1);

                if (!isNumber(LHS) || !isNumber(RHS)) {
                    NDebug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                try {
                    from = std::stoll(LHS);
                    to   = std::stoll(RHS);
                } catch (std::exception& e) {
                    NDebug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                if (to < from || to < 1 || from < 1) {
                    NDebug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                if (std::clamp(m_iID, from, to) != m_iID)
                    return false;
                continue;
            }

            if (cur == 's') {
                if (!prop.starts_with("s[") || !prop.ends_with("]")) {
                    NDebug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                prop = prop.substr(2, prop.length() - 3);

                const auto SHOULDBESPECIAL = configStringToInt(prop);

                if (SHOULDBESPECIAL && (bool)*SHOULDBESPECIAL != m_bIsSpecialWorkspace)
                    return false;
                continue;
            }

            if (cur == 'm') {
                if (!prop.starts_with("m[") || !prop.ends_with("]")) {
                    NDebug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                prop = prop.substr(2, prop.length() - 3);

                const auto PMONITOR = g_pCompositor->getMonitorFromString(prop);

                if (!(PMONITOR ? PMONITOR == m_pMonitor : false))
                    return false;
                continue;
            }

            if (cur == 'n') {
                if (!prop.starts_with("n[") || !prop.ends_with("]")) {
                    NDebug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                prop = prop.substr(2, prop.length() - 3);

                if (prop.starts_with("s:") && !m_szName.starts_with(prop.substr(2)))
                    return false;
                if (prop.starts_with("e:") && !m_szName.ends_with(prop.substr(2)))
                    return false;

                const auto WANTSNAMED = configStringToInt(prop);

                if (WANTSNAMED && *WANTSNAMED != (m_iID <= -1337))
                    return false;
                continue;
            }

            if (cur == 'w') {
                WORKSPACEID from = 0, to = 0;
                if (!prop.starts_with("w[") || !prop.ends_with("]")) {
                    NDebug::log(LOG, "Invalid selector {}", selector);
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
                        NDebug::log(LOG, "Invalid selector {}", selector);
                        return false;
                    }

                    try {
                        from = std::stoll(prop);
                    } catch (std::exception& e) {
                        NDebug::log(LOG, "Invalid selector {}", selector);
                        return false;
                    }

                    int count;
                    if (wantsCountGroup)
                        count = getGroups(wantsOnlyTiled == -1 ? std::nullopt : std::optional<bool>((bool)wantsOnlyTiled),
                                          wantsOnlyPinned ? std::optional<bool>(wantsOnlyPinned) : std::nullopt,
                                          wantsCountVisible ? std::optional<bool>(wantsCountVisible) : std::nullopt);
                    else
                        count = getWindows(wantsOnlyTiled == -1 ? std::nullopt : std::optional<bool>((bool)wantsOnlyTiled),
                                           wantsOnlyPinned ? std::optional<bool>(wantsOnlyPinned) : std::nullopt,
                                           wantsCountVisible ? std::optional<bool>(wantsCountVisible) : std::nullopt);

                    if (count != from)
                        return false;
                    continue;
                }

                const auto DASHPOS = prop.find('-');
                const auto LHS = prop.substr(0, DASHPOS), RHS = prop.substr(DASHPOS + 1);

                if (!isNumber(LHS) || !isNumber(RHS)) {
                    NDebug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                try {
                    from = std::stoll(LHS);
                    to   = std::stoll(RHS);
                } catch (std::exception& e) {
                    NDebug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                if (to < from || to < 1 || from < 1) {
                    NDebug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                WORKSPACEID count;
                if (wantsCountGroup)
                    count =
                        getGroups(wantsOnlyTiled == -1 ? std::nullopt : std::optional<bool>((bool)wantsOnlyTiled),
                                  wantsOnlyPinned ? std::optional<bool>(wantsOnlyPinned) : std::nullopt, wantsCountVisible ? std::optional<bool>(wantsCountVisible) : std::nullopt);
                else
                    count = getWindows(wantsOnlyTiled == -1 ? std::nullopt : std::optional<bool>((bool)wantsOnlyTiled),
                                       wantsOnlyPinned ? std::optional<bool>(wantsOnlyPinned) : std::nullopt,
                                       wantsCountVisible ? std::optional<bool>(wantsCountVisible) : std::nullopt);

                if (std::clamp(count, from, to) != count)
                    return false;
                continue;
            }

            if (cur == 'f') {
                if (!prop.starts_with("f[") || !prop.ends_with("]")) {
                    NDebug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                prop        = prop.substr(2, prop.length() - 3);
                int FSSTATE = -1;
                try {
                    FSSTATE = std::stoi(prop);
                } catch (std::exception& e) {
                    NDebug::log(LOG, "Invalid selector {}", selector);
                    return false;
                }

                switch (FSSTATE) {
                    case -1: // no fullscreen
                        if (m_bHasFullscreenWindow)
                            return false;
                        break;
                    case 0: // fullscreen full
                        if (!m_bHasFullscreenWindow || m_efFullscreenMode != FSMODE_FULLSCREEN)
                            return false;
                        break;
                    case 1: // maximized
                        if (!m_bHasFullscreenWindow || m_efFullscreenMode != FSMODE_MAXIMIZED)
                            return false;
                        break;
                    default: break;
                }
                continue;
            }

            NDebug::log(LOG, "Invalid selector {}", selector);
            return false;
        }

        return true;
    }

    UNREACHABLE();
    return false;
}

void CWorkspace::markInert() {
    m_bInert   = true;
    m_iID      = WORKSPACE_INVALID;
    m_bVisible = false;
    m_pMonitor.reset();
}

bool CWorkspace::inert() {
    return m_bInert;
}

MONITORID CWorkspace::monitorID() {
    return m_pMonitor ? m_pMonitor->ID : MONITOR_INVALID;
}

PHLWINDOW CWorkspace::getFullscreenWindow() {
    for (auto const& w : g_pCompositor->m_vWindows) {
        if (w->m_pWorkspace == m_pSelf && w->isFullscreen())
            return w;
    }

    return nullptr;
}

bool CWorkspace::isVisible() {
    return m_bVisible;
}

bool CWorkspace::isVisibleNotCovered() {
    const auto PMONITOR = m_pMonitor.lock();
    if (PMONITOR->activeSpecialWorkspace)
        return PMONITOR->activeSpecialWorkspace->m_iID == m_iID;

    return PMONITOR->activeWorkspace->m_iID == m_iID;
}

int CWorkspace::getWindows(std::optional<bool> onlyTiled, std::optional<bool> onlyPinned, std::optional<bool> onlyVisible) {
    int no = 0;
    for (auto const& w : g_pCompositor->m_vWindows) {
        if (w->workspaceID() != m_iID || !w->m_bIsMapped)
            continue;
        if (onlyTiled.has_value() && w->m_bIsFloating == onlyTiled.value())
            continue;
        if (onlyPinned.has_value() && w->m_bPinned != onlyPinned.value())
            continue;
        if (onlyVisible.has_value() && w->isHidden() == onlyVisible.value())
            continue;
        no++;
    }

    return no;
}

int CWorkspace::getGroups(std::optional<bool> onlyTiled, std::optional<bool> onlyPinned, std::optional<bool> onlyVisible) {
    int no = 0;
    for (auto const& w : g_pCompositor->m_vWindows) {
        if (w->workspaceID() != m_iID || !w->m_bIsMapped)
            continue;
        if (!w->m_sGroupData.head)
            continue;
        if (onlyTiled.has_value() && w->m_bIsFloating == onlyTiled.value())
            continue;
        if (onlyPinned.has_value() && w->m_bPinned != onlyPinned.value())
            continue;
        if (onlyVisible.has_value() && w->isHidden() == onlyVisible.value())
            continue;
        no++;
    }
    return no;
}

PHLWINDOW CWorkspace::getFirstWindow() {
    for (auto const& w : g_pCompositor->m_vWindows) {
        if (w->m_pWorkspace == m_pSelf && w->m_bIsMapped && !w->isHidden())
            return w;
    }

    return nullptr;
}

PHLWINDOW CWorkspace::getTopLeftWindow() {
    const auto PMONITOR = m_pMonitor.lock();

    for (auto const& w : g_pCompositor->m_vWindows) {
        if (w->m_pWorkspace != m_pSelf || !w->m_bIsMapped || w->isHidden())
            continue;

        const auto WINDOWIDEALBB = w->getWindowIdealBoundingBoxIgnoreReserved();

        if (WINDOWIDEALBB.x <= PMONITOR->vecPosition.x + 1 && WINDOWIDEALBB.y <= PMONITOR->vecPosition.y + 1)
            return w;
    }
    return nullptr;
}

bool CWorkspace::hasUrgentWindow() {
    for (auto const& w : g_pCompositor->m_vWindows) {
        if (w->m_pWorkspace == m_pSelf && w->m_bIsMapped && w->m_bIsUrgent)
            return true;
    }

    return false;
}

void CWorkspace::updateWindowDecos() {
    for (auto const& w : g_pCompositor->m_vWindows) {
        if (w->m_pWorkspace != m_pSelf)
            continue;

        w->updateWindowDecos();
    }
}

void CWorkspace::updateWindowData() {
    const auto WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(m_pSelf.lock());

    for (auto const& w : g_pCompositor->m_vWindows) {
        if (w->m_pWorkspace != m_pSelf)
            continue;

        w->updateWindowData(WORKSPACERULE);
    }
}

void CWorkspace::forceReportSizesToWindows() {
    for (auto const& w : g_pCompositor->m_vWindows) {
        if (w->m_pWorkspace != m_pSelf || !w->m_bIsMapped || w->isHidden())
            continue;

        w->sendWindowSize(true);
    }
}

void CWorkspace::rename(const std::string& name) {
    if (g_pCompositor->isWorkspaceSpecial(m_iID))
        return;

    NDebug::log(LOG, "CWorkspace::rename: Renaming workspace {} to '{}'", m_iID, name);
    m_szName = name;

    const auto WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(m_pSelf.lock());
    m_bPersistent            = WORKSPACERULE.isPersistent;

    if (WORKSPACERULE.isPersistent)
        g_pCompositor->ensurePersistentWorkspacesPresent(std::vector<SWorkspaceRule>{WORKSPACERULE}, m_pSelf.lock());

    g_pEventManager->postEvent({"renameworkspace", std::to_string(m_iID) + "," + m_szName});
}

void CWorkspace::updateWindows() {
    m_bHasFullscreenWindow = std::ranges::any_of(g_pCompositor->m_vWindows, [this](const auto& w) { return w->m_bIsMapped && w->m_pWorkspace == m_pSelf && w->isFullscreen(); });

    for (auto const& w : g_pCompositor->m_vWindows) {
        if (!w->m_bIsMapped || w->m_pWorkspace != m_pSelf)
            continue;

        w->updateDynamicRules();
    }
}
