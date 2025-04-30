#include "InputManager.hpp"
#include "../../Compositor.hpp"
#include "../../desktop/LayerSurface.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../managers/HookSystemManager.hpp"
#include "../../render/Renderer.hpp"

void CInputManager::onSwipeBegin(IPointer::SSwipeBeginEvent e) {
    static auto PSWIPE           = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe");
    static auto PSWIPEFINGERS    = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_fingers");
    static auto PSWIPEMINFINGERS = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_min_fingers");
    static auto PSWIPENEW        = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_create_new");

    EMIT_HOOK_EVENT_CANCELLABLE("swipeBegin", e);

    if ((!*PSWIPEMINFINGERS && e.fingers != *PSWIPEFINGERS) || (*PSWIPEMINFINGERS && e.fingers < *PSWIPEFINGERS) || *PSWIPE == 0 || g_pSessionLockManager->isSessionLocked())
        return;

    int onMonitor = 0;
    for (auto const& w : g_pCompositor->m_workspaces) {
        if (w->m_monitor == g_pCompositor->m_lastMonitor && !g_pCompositor->isWorkspaceSpecial(w->m_id))
            onMonitor++;
    }

    if (onMonitor < 2 && !*PSWIPENEW)
        return; // disallow swiping when there's 1 workspace on a monitor

    beginWorkspaceSwipe();
}

void CInputManager::beginWorkspaceSwipe() {
    const auto PWORKSPACE = g_pCompositor->m_lastMonitor->m_activeWorkspace;

    Debug::log(LOG, "Starting a swipe from {}", PWORKSPACE->m_name);

    m_sActiveSwipe.pWorkspaceBegin = PWORKSPACE;
    m_sActiveSwipe.delta           = 0;
    m_sActiveSwipe.pMonitor        = g_pCompositor->m_lastMonitor;
    m_sActiveSwipe.avgSpeed        = 0;
    m_sActiveSwipe.speedPoints     = 0;

    if (PWORKSPACE->m_hasFullscreenWindow) {
        for (auto const& ls : g_pCompositor->m_lastMonitor->m_layerSurfaceLayers[2]) {
            *ls->m_alpha = 1.f;
        }
    }
}

void CInputManager::onSwipeEnd(IPointer::SSwipeEndEvent e) {
    EMIT_HOOK_EVENT_CANCELLABLE("swipeEnd", e);

    if (!m_sActiveSwipe.pWorkspaceBegin)
        return; // no valid swipe
    endWorkspaceSwipe();
}

void CInputManager::endWorkspaceSwipe() {
    static auto PSWIPEPERC    = CConfigValue<Hyprlang::FLOAT>("gestures:workspace_swipe_cancel_ratio");
    static auto PSWIPEDIST    = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_distance");
    static auto PSWIPEFORC    = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_min_speed_to_force");
    static auto PSWIPENEW     = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_create_new");
    static auto PSWIPEUSER    = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_use_r");
    static auto PWORKSPACEGAP = CConfigValue<Hyprlang::INT>("general:gaps_workspaces");
    const auto  ANIMSTYLE     = m_sActiveSwipe.pWorkspaceBegin->m_renderOffset->getStyle();
    const bool  VERTANIMS     = ANIMSTYLE == "slidevert" || ANIMSTYLE.starts_with("slidefadevert");

    // commit
    auto       workspaceIDLeft  = getWorkspaceIDNameFromString((*PSWIPEUSER ? "r-1" : "m-1")).id;
    auto       workspaceIDRight = getWorkspaceIDNameFromString((*PSWIPEUSER ? "r+1" : "m+1")).id;
    const auto SWIPEDISTANCE    = std::clamp(*PSWIPEDIST, (int64_t)1LL, (int64_t)UINT32_MAX);

    // If we've been swiping off the right end with PSWIPENEW enabled, there is
    // no workspace there yet, and we need to choose an ID for a new one now.
    if (workspaceIDRight <= m_sActiveSwipe.pWorkspaceBegin->m_id && *PSWIPENEW) {
        workspaceIDRight = getWorkspaceIDNameFromString("r+1").id;
    }

    auto         PWORKSPACER = g_pCompositor->getWorkspaceByID(workspaceIDRight); // not guaranteed if PSWIPENEW || PSWIPENUMBER
    auto         PWORKSPACEL = g_pCompositor->getWorkspaceByID(workspaceIDLeft);  // not guaranteed if PSWIPENUMBER

    const auto   RENDEROFFSETMIDDLE = m_sActiveSwipe.pWorkspaceBegin->m_renderOffset->value();
    const auto   XDISTANCE          = m_sActiveSwipe.pMonitor->m_size.x + *PWORKSPACEGAP;
    const auto   YDISTANCE          = m_sActiveSwipe.pMonitor->m_size.y + *PWORKSPACEGAP;

    PHLWORKSPACE pSwitchedTo = nullptr;

    if ((abs(m_sActiveSwipe.delta) < SWIPEDISTANCE * *PSWIPEPERC && (*PSWIPEFORC == 0 || (*PSWIPEFORC != 0 && m_sActiveSwipe.avgSpeed < *PSWIPEFORC))) ||
        abs(m_sActiveSwipe.delta) < 2) {
        // revert
        if (abs(m_sActiveSwipe.delta) < 2) {
            if (PWORKSPACEL)
                PWORKSPACEL->m_renderOffset->setValueAndWarp(Vector2D(0, 0));
            if (PWORKSPACER)
                PWORKSPACER->m_renderOffset->setValueAndWarp(Vector2D(0, 0));
            m_sActiveSwipe.pWorkspaceBegin->m_renderOffset->setValueAndWarp(Vector2D(0, 0));
        } else {
            if (m_sActiveSwipe.delta < 0) {
                // to left

                if (PWORKSPACEL) {
                    if (VERTANIMS)
                        *PWORKSPACEL->m_renderOffset = Vector2D{0.0, -YDISTANCE};
                    else
                        *PWORKSPACEL->m_renderOffset = Vector2D{-XDISTANCE, 0.0};
                }
            } else if (PWORKSPACER) {
                // to right
                if (VERTANIMS)
                    *PWORKSPACER->m_renderOffset = Vector2D{0.0, YDISTANCE};
                else
                    *PWORKSPACER->m_renderOffset = Vector2D{XDISTANCE, 0.0};
            }

            *m_sActiveSwipe.pWorkspaceBegin->m_renderOffset = Vector2D();
        }

        pSwitchedTo = m_sActiveSwipe.pWorkspaceBegin;
    } else if (m_sActiveSwipe.delta < 0) {
        // switch to left
        const auto RENDEROFFSET = PWORKSPACEL ? PWORKSPACEL->m_renderOffset->value() : Vector2D();

        if (PWORKSPACEL)
            m_sActiveSwipe.pMonitor->changeWorkspace(workspaceIDLeft);
        else {
            m_sActiveSwipe.pMonitor->changeWorkspace(g_pCompositor->createNewWorkspace(workspaceIDLeft, m_sActiveSwipe.pMonitor->m_id));
            PWORKSPACEL = g_pCompositor->getWorkspaceByID(workspaceIDLeft);
            PWORKSPACEL->rememberPrevWorkspace(m_sActiveSwipe.pWorkspaceBegin);
        }

        PWORKSPACEL->m_renderOffset->setValue(RENDEROFFSET);
        PWORKSPACEL->m_alpha->setValueAndWarp(1.f);

        m_sActiveSwipe.pWorkspaceBegin->m_renderOffset->setValue(RENDEROFFSETMIDDLE);
        if (VERTANIMS)
            *m_sActiveSwipe.pWorkspaceBegin->m_renderOffset = Vector2D(0.0, YDISTANCE);
        else
            *m_sActiveSwipe.pWorkspaceBegin->m_renderOffset = Vector2D(XDISTANCE, 0.0);
        m_sActiveSwipe.pWorkspaceBegin->m_alpha->setValueAndWarp(1.f);

        g_pInputManager->unconstrainMouse();

        Debug::log(LOG, "Ended swipe to the left");

        pSwitchedTo = PWORKSPACEL;
    } else {
        // switch to right
        const auto RENDEROFFSET = PWORKSPACER ? PWORKSPACER->m_renderOffset->value() : Vector2D();

        if (PWORKSPACER)
            m_sActiveSwipe.pMonitor->changeWorkspace(workspaceIDRight);
        else {
            m_sActiveSwipe.pMonitor->changeWorkspace(g_pCompositor->createNewWorkspace(workspaceIDRight, m_sActiveSwipe.pMonitor->m_id));
            PWORKSPACER = g_pCompositor->getWorkspaceByID(workspaceIDRight);
            PWORKSPACER->rememberPrevWorkspace(m_sActiveSwipe.pWorkspaceBegin);
        }

        PWORKSPACER->m_renderOffset->setValue(RENDEROFFSET);
        PWORKSPACER->m_alpha->setValueAndWarp(1.f);

        m_sActiveSwipe.pWorkspaceBegin->m_renderOffset->setValue(RENDEROFFSETMIDDLE);
        if (VERTANIMS)
            *m_sActiveSwipe.pWorkspaceBegin->m_renderOffset = Vector2D(0.0, -YDISTANCE);
        else
            *m_sActiveSwipe.pWorkspaceBegin->m_renderOffset = Vector2D(-XDISTANCE, 0.0);
        m_sActiveSwipe.pWorkspaceBegin->m_alpha->setValueAndWarp(1.f);

        g_pInputManager->unconstrainMouse();

        Debug::log(LOG, "Ended swipe to the right");

        pSwitchedTo = PWORKSPACER;
    }
    m_sActiveSwipe.pWorkspaceBegin->rememberPrevWorkspace(pSwitchedTo);

    g_pHyprRenderer->damageMonitor(m_sActiveSwipe.pMonitor.lock());

    if (PWORKSPACEL)
        PWORKSPACEL->m_forceRendering = false;
    if (PWORKSPACER)
        PWORKSPACER->m_forceRendering = false;
    m_sActiveSwipe.pWorkspaceBegin->m_forceRendering = false;

    m_sActiveSwipe.pWorkspaceBegin  = nullptr;
    m_sActiveSwipe.initialDirection = 0;

    g_pInputManager->refocus();

    // apply alpha
    for (auto const& ls : g_pCompositor->m_lastMonitor->m_layerSurfaceLayers[2]) {
        *ls->m_alpha = pSwitchedTo->m_hasFullscreenWindow && pSwitchedTo->m_fullscreenMode == FSMODE_FULLSCREEN ? 0.f : 1.f;
    }
}

void CInputManager::onSwipeUpdate(IPointer::SSwipeUpdateEvent e) {
    EMIT_HOOK_EVENT_CANCELLABLE("swipeUpdate", e);

    if (!m_sActiveSwipe.pWorkspaceBegin)
        return;
    static auto  PSWIPEINVR = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_invert");
    const auto   ANIMSTYLE  = m_sActiveSwipe.pWorkspaceBegin->m_renderOffset->getStyle();
    const bool   VERTANIMS  = ANIMSTYLE == "slidevert" || ANIMSTYLE.starts_with("slidefadevert");

    const double delta = m_sActiveSwipe.delta + (VERTANIMS ? (*PSWIPEINVR ? -e.delta.y : e.delta.y) : (*PSWIPEINVR ? -e.delta.x : e.delta.x));
    updateWorkspaceSwipe(delta);
}

void CInputManager::updateWorkspaceSwipe(double delta) {
    static auto  PSWIPEDIST             = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_distance");
    static auto  PSWIPENEW              = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_create_new");
    static auto  PSWIPEDIRLOCK          = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_direction_lock");
    static auto  PSWIPEDIRLOCKTHRESHOLD = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_direction_lock_threshold");
    static auto  PSWIPEFOREVER          = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_forever");
    static auto  PSWIPEUSER             = CConfigValue<Hyprlang::INT>("gestures:workspace_swipe_use_r");
    static auto  PWORKSPACEGAP          = CConfigValue<Hyprlang::INT>("general:gaps_workspaces");

    const auto   SWIPEDISTANCE = std::clamp(*PSWIPEDIST, (int64_t)1LL, (int64_t)UINT32_MAX);
    const auto   XDISTANCE     = m_sActiveSwipe.pMonitor->m_size.x + *PWORKSPACEGAP;
    const auto   YDISTANCE     = m_sActiveSwipe.pMonitor->m_size.y + *PWORKSPACEGAP;
    const auto   ANIMSTYLE     = m_sActiveSwipe.pWorkspaceBegin->m_renderOffset->getStyle();
    const bool   VERTANIMS     = ANIMSTYLE == "slidevert" || ANIMSTYLE.starts_with("slidefadevert");
    const double d             = m_sActiveSwipe.delta - delta;
    m_sActiveSwipe.delta       = delta;

    m_sActiveSwipe.avgSpeed = (m_sActiveSwipe.avgSpeed * m_sActiveSwipe.speedPoints + abs(d)) / (m_sActiveSwipe.speedPoints + 1);
    m_sActiveSwipe.speedPoints++;

    auto workspaceIDLeft  = getWorkspaceIDNameFromString((*PSWIPEUSER ? "r-1" : "m-1")).id;
    auto workspaceIDRight = getWorkspaceIDNameFromString((*PSWIPEUSER ? "r+1" : "m+1")).id;

    if ((workspaceIDLeft == WORKSPACE_INVALID || workspaceIDRight == WORKSPACE_INVALID || workspaceIDLeft == m_sActiveSwipe.pWorkspaceBegin->m_id) && !*PSWIPENEW) {
        m_sActiveSwipe.pWorkspaceBegin = nullptr; // invalidate the swipe
        return;
    }

    m_sActiveSwipe.pWorkspaceBegin->m_forceRendering = true;

    m_sActiveSwipe.delta = std::clamp(m_sActiveSwipe.delta, (double)-SWIPEDISTANCE, (double)SWIPEDISTANCE);

    if ((m_sActiveSwipe.pWorkspaceBegin->m_id == workspaceIDLeft && *PSWIPENEW && (m_sActiveSwipe.delta < 0)) ||
        (m_sActiveSwipe.delta > 0 && m_sActiveSwipe.pWorkspaceBegin->getWindows() == 0 && workspaceIDRight <= m_sActiveSwipe.pWorkspaceBegin->m_id) ||
        (m_sActiveSwipe.delta < 0 && m_sActiveSwipe.pWorkspaceBegin->m_id <= workspaceIDLeft)) {

        m_sActiveSwipe.delta = 0;
        return;
    }

    if (*PSWIPEDIRLOCK) {
        if (m_sActiveSwipe.initialDirection != 0 && m_sActiveSwipe.initialDirection != (m_sActiveSwipe.delta < 0 ? -1 : 1))
            m_sActiveSwipe.delta = 0;
        else if (m_sActiveSwipe.initialDirection == 0 && abs(m_sActiveSwipe.delta) > *PSWIPEDIRLOCKTHRESHOLD)
            m_sActiveSwipe.initialDirection = m_sActiveSwipe.delta < 0 ? -1 : 1;
    }

    if (m_sActiveSwipe.delta < 0) {
        const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(workspaceIDLeft);

        if (workspaceIDLeft > m_sActiveSwipe.pWorkspaceBegin->m_id || !PWORKSPACE) {
            if (*PSWIPENEW) {
                g_pHyprRenderer->damageMonitor(m_sActiveSwipe.pMonitor.lock());

                if (VERTANIMS)
                    m_sActiveSwipe.pWorkspaceBegin->m_renderOffset->setValueAndWarp(Vector2D(0.0, ((-m_sActiveSwipe.delta) / SWIPEDISTANCE) * YDISTANCE));
                else
                    m_sActiveSwipe.pWorkspaceBegin->m_renderOffset->setValueAndWarp(Vector2D(((-m_sActiveSwipe.delta) / SWIPEDISTANCE) * XDISTANCE, 0.0));

                m_sActiveSwipe.pWorkspaceBegin->updateWindowDecos();
                return;
            }
            m_sActiveSwipe.delta = 0;
            return;
        }

        PWORKSPACE->m_forceRendering = true;
        PWORKSPACE->m_alpha->setValueAndWarp(1.f);

        if (workspaceIDLeft != workspaceIDRight && workspaceIDRight != m_sActiveSwipe.pWorkspaceBegin->m_id) {
            const auto PWORKSPACER = g_pCompositor->getWorkspaceByID(workspaceIDRight);

            if (PWORKSPACER) {
                PWORKSPACER->m_forceRendering = false;
                PWORKSPACER->m_alpha->setValueAndWarp(0.f);
            }
        }

        if (VERTANIMS) {
            PWORKSPACE->m_renderOffset->setValueAndWarp(Vector2D(0.0, ((-m_sActiveSwipe.delta) / SWIPEDISTANCE) * YDISTANCE - YDISTANCE));
            m_sActiveSwipe.pWorkspaceBegin->m_renderOffset->setValueAndWarp(Vector2D(0.0, ((-m_sActiveSwipe.delta) / SWIPEDISTANCE) * YDISTANCE));
        } else {
            PWORKSPACE->m_renderOffset->setValueAndWarp(Vector2D(((-m_sActiveSwipe.delta) / SWIPEDISTANCE) * XDISTANCE - XDISTANCE, 0.0));
            m_sActiveSwipe.pWorkspaceBegin->m_renderOffset->setValueAndWarp(Vector2D(((-m_sActiveSwipe.delta) / SWIPEDISTANCE) * XDISTANCE, 0.0));
        }

        PWORKSPACE->updateWindowDecos();
    } else {
        const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(workspaceIDRight);

        if (workspaceIDRight < m_sActiveSwipe.pWorkspaceBegin->m_id || !PWORKSPACE) {
            if (*PSWIPENEW) {
                g_pHyprRenderer->damageMonitor(m_sActiveSwipe.pMonitor.lock());

                if (VERTANIMS)
                    m_sActiveSwipe.pWorkspaceBegin->m_renderOffset->setValueAndWarp(Vector2D(0.0, ((-m_sActiveSwipe.delta) / SWIPEDISTANCE) * YDISTANCE));
                else
                    m_sActiveSwipe.pWorkspaceBegin->m_renderOffset->setValueAndWarp(Vector2D(((-m_sActiveSwipe.delta) / SWIPEDISTANCE) * XDISTANCE, 0.0));

                m_sActiveSwipe.pWorkspaceBegin->updateWindowDecos();
                return;
            }
            m_sActiveSwipe.delta = 0;
            return;
        }

        PWORKSPACE->m_forceRendering = true;
        PWORKSPACE->m_alpha->setValueAndWarp(1.f);

        if (workspaceIDLeft != workspaceIDRight && workspaceIDLeft != m_sActiveSwipe.pWorkspaceBegin->m_id) {
            const auto PWORKSPACEL = g_pCompositor->getWorkspaceByID(workspaceIDLeft);

            if (PWORKSPACEL) {
                PWORKSPACEL->m_forceRendering = false;
                PWORKSPACEL->m_alpha->setValueAndWarp(0.f);
            }
        }

        if (VERTANIMS) {
            PWORKSPACE->m_renderOffset->setValueAndWarp(Vector2D(0.0, ((-m_sActiveSwipe.delta) / SWIPEDISTANCE) * YDISTANCE + YDISTANCE));
            m_sActiveSwipe.pWorkspaceBegin->m_renderOffset->setValueAndWarp(Vector2D(0.0, ((-m_sActiveSwipe.delta) / SWIPEDISTANCE) * YDISTANCE));
        } else {
            PWORKSPACE->m_renderOffset->setValueAndWarp(Vector2D(((-m_sActiveSwipe.delta) / SWIPEDISTANCE) * XDISTANCE + XDISTANCE, 0.0));
            m_sActiveSwipe.pWorkspaceBegin->m_renderOffset->setValueAndWarp(Vector2D(((-m_sActiveSwipe.delta) / SWIPEDISTANCE) * XDISTANCE, 0.0));
        }

        PWORKSPACE->updateWindowDecos();
    }

    g_pHyprRenderer->damageMonitor(m_sActiveSwipe.pMonitor.lock());

    m_sActiveSwipe.pWorkspaceBegin->updateWindowDecos();

    if (*PSWIPEFOREVER) {
        if (abs(m_sActiveSwipe.delta) >= SWIPEDISTANCE) {
            onSwipeEnd({});
            beginWorkspaceSwipe();
        }
    }
}
