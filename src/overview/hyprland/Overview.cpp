#include "Overview.hpp"

#include "config/shared/actions/ConfigActions.hpp"
#include "desktop/state/FocusState.hpp"
#include "desktop/view/window/Window.hpp"
#include "devices/IPointer.hpp"
#include "input/Keys.hpp"
#include "managers/eventLoop/EventLoopTimer.hpp"
#include "managers/input/InputManager.hpp"
#include "overview/hyprland/scene/WorkspaceTapeController.hpp"
#include "scene/OverviewScene.hpp"
#include "scene/WorkspaceTapeController.hpp"
#include "../../animation/AnimationManager.hpp"
#include "../../config/shared/animation/AnimationTree.hpp"
#include "../../event/EventBus.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../../devices/IKeyboard.hpp"
#include "../../managers/SessionLockManager.hpp"
#include "../../output/Monitor.hpp"
#include "../../output/MonitorResources.hpp"
#include "../../render/Renderer.hpp"
#include "../../layout/LayoutManager.hpp"
#include "../../layout/supplementary/DragController.hpp"

#include <chrono>
#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <ranges>
#include <wayland-server-protocol.h>
#include <xkbcommon/xkbcommon-keysyms.h>

using namespace Overview;
using namespace Overview::Hyprland;

constexpr const float DRAG_MOVE_MS = 500.F;

COverview::COverview() : m_scene(makeShared<COverviewScene>(*this)) {
    ;
}

COverview::~COverview() {
    m_isOpen = false;
    if (m_progress)
        m_progress->resetAllCallbacks();
    finishClose(false);
}

void COverview::installListeners() {
    m_listeners.keyboardKey = Event::bus()->m_events.input.keyboard.key.listen([this](const IKeyboard::SKeyEvent& event, SP<IKeyboard> keyboard, Event::SCallbackInfo& info) {
        const auto INTERCEPTED = [&] {
            return std::ranges::find_if(m_interceptedKeys, [&event, &keyboard](const auto& key) { return key.first == keyboard && key.second == event.keycode; });
        };

        if (event.state == WL_KEYBOARD_KEY_STATE_RELEASED) {
            const auto IT = INTERCEPTED();
            if (IT == m_interceptedKeys.end())
                return;

            m_interceptedKeys.erase(IT);
            info.cancelled = true;
            return;
        }

        if (!m_isOpen || !keyboard || !keyboard->m_xkbSymState)
            return;

        const auto KEYSYM = xkb_state_key_get_one_sym(keyboard->m_xkbSymState, event.keycode + 8);
        if (KEYSYM != XKB_KEY_Left && KEYSYM != XKB_KEY_Right && KEYSYM != XKB_KEY_h && KEYSYM != XKB_KEY_l)
            return;

        info.cancelled = true;
        if (INTERCEPTED() == m_interceptedKeys.end())
            m_interceptedKeys.emplace_back(keyboard, event.keycode);
        if (KEYSYM == XKB_KEY_Left || KEYSYM == XKB_KEY_h)
            m_scene->navigateLeft();
        else
            m_scene->navigateRight();
    });

    m_listeners.monitorDisconnect  = m_monitor->m_events.disconnect.listen([this] { closeImmediately(); });
    m_listeners.monitorModeChanged = m_monitor->m_events.modeChanged.listen([this] { closeImmediately(); });
    m_listeners.monitorPreRender   = Event::bus()->m_events.render.preChecks.listen([this](PHLMONITOR monitor) {
        if (monitor != m_monitor.lock() || monitor->resources() == m_resources)
            return;

        closeImmediately();
    });

    if (g_pSessionLockManager)
        m_listeners.sessionLock = g_pSessionLockManager->m_events.lock.listen([this] { closeImmediately(); });

    m_listeners.mouseButton = Event::bus()->m_events.input.mouse.button.listen([this](IPointer::SButtonEvent e, Event::SCallbackInfo& i) {
        if (e.state != WL_POINTER_BUTTON_STATE_PRESSED)
            return;

        if (!m_monitor)
            return;

        if (!m_monitor->logicalBox().containsPoint(g_pInputManager->getMouseCoordsInternal()))
            return;

        // TODO: make this better. This is to support drags, and is obviously kinda wrong.
        if (g_pInputManager->getModsFromAllKBs() != Input::eKeyboardModifiers::HL_MODIFIER_NONE)
            return;

        close();

        i.cancelled = true;
    });

    m_listeners.mouseMove = Event::bus()->m_events.input.mouse.move.listen([this](Vector2D p, Event::SCallbackInfo& i) { recheckDrag(); });
}

void COverview::recheckDrag() {
    auto resetDrag = [this] {
        m_drag.isWithin = false;
        if (m_drag.eventLoopTimer)
            m_drag.eventLoopTimer->cancel();
    };

    if (!m_monitor) {
        resetDrag();
        return;
    }

    if (!m_monitor->logicalBox().containsPoint(g_pInputManager->getMouseCoordsInternal())) {
        resetDrag();
        return;
    }

    const auto TARGET = g_layoutManager->dragController()->target();

    if (!TARGET) {
        resetDrag();
        return;
    }

    const auto MONBOX                   = m_monitor->logicalBox();
    const auto MOUSE_LOCAL              = g_pInputManager->getMouseCoordsInternal() - MONBOX.pos();
    const auto SIDE_WIDTH               = (MONBOX.size().x - (MONBOX.size() * CWorkspaceTapeController::TILE_SCALE).x) / 2.F;
    const bool MOUSE_IS_LEFT_DIST       = MOUSE_LOCAL.x < SIDE_WIDTH;
    const auto IS_MOUSE_WITHIN_DISTANCE = MOUSE_IS_LEFT_DIST || MOUSE_LOCAL.x > MONBOX.size().x - SIDE_WIDTH;

    if (!IS_MOUSE_WITHIN_DISTANCE) {
        resetDrag();
        return;
    }

    if (IS_MOUSE_WITHIN_DISTANCE && m_drag.isWithin) {
        // check if timer passed, if so, move
        if (m_drag.debouncer.getMillis() < DRAG_MOVE_MS)
            return;

        if (MOUSE_IS_LEFT_DIST)
            m_scene->navigateLeft();
        else
            m_scene->navigateRight();

        // set the current dragger's workspace MOTHERFUCKER
        if (TARGET->window()) {
            TARGET->window()->moveToWorkspace(m_scene->selectedWorkspace());
            g_layoutManager->dragController()->overrideDragWindowTargetWS(m_scene->selectedWorkspace());
        }

        m_drag.debouncer.reset();
        return;
    }

    m_drag.isWithin = true;
    m_drag.debouncer.reset();

    if (!m_drag.eventLoopTimer) {
        m_drag.eventLoopTimer =
            makeShared<CEventLoopTimer>(std::chrono::milliseconds(sc<int32_t>(DRAG_MOVE_MS) + 10), [this](SP<CEventLoopTimer> self, void* data) { recheckDrag(); }, nullptr);
        g_pEventLoopManager->addTimer(m_drag.eventLoopTimer);
    } else
        m_drag.eventLoopTimer->updateTimeout(std::chrono::milliseconds(sc<int32_t>(DRAG_MOVE_MS) + 10));
}

void COverview::open(PHLMONITOR monitor) {
    if (!monitor || !monitor->m_enabled || monitor->isMirror() || !g_pHyprRenderer || (g_pSessionLockManager && g_pSessionLockManager->isSessionLocked()))
        return;

    if (!monitor->m_lastScanout.expired() || monitor->m_directScanoutIsActive)
        monitor->handleDSleave();

    const auto CURRENT_MONITOR = m_monitor.lock();
    if (m_sceneInstalled && CURRENT_MONITOR != monitor) {
        m_isOpen = false;
        finishClose();
    }

    if (m_isOpen)
        return;

    if (!m_progress) {
        Animation::mgr()->createAnimation(0.F, m_progress, Config::animationTree()->getAnimationPropertyConfig("overviewIn"), AVARDAMAGE_NONE);
        m_progress->setUpdateCallback([this](auto) {
            if (const auto MONITOR = m_monitor.lock(); MONITOR && g_pHyprRenderer)
                g_pHyprRenderer->damageMonitor(MONITOR);
        });
        m_progress->setCallbackOnEnd(
            [this](auto) {
                if (m_isOpen)
                    return;

                if (g_pEventLoopManager)
                    g_pEventLoopManager->doLater([this] { finishClose(); });
                else
                    finishClose();
            },
            false);
    }

    const bool NEW_SCENE = !m_sceneInstalled;
    const auto RESOURCES = NEW_SCENE ? monitor->resources() : m_resources;
    if (!RESOURCES)
        return;

    m_isOpen = true;
    if (NEW_SCENE) {
        m_monitor   = monitor;
        m_resources = RESOURCES;
        m_scene->start(monitor, RESOURCES);
        RESOURCES->m_sceneStack.push(m_scene);
        m_sceneInstalled = true;
        m_progress->setValueAndWarp(0.F);

        installListeners();
    }

    m_progress->setConfig(Config::animationTree()->getAnimationPropertyConfig("overviewIn"));
    *m_progress = 1.F;

    monitor->recheckSolitary();
    if (g_pHyprRenderer)
        g_pHyprRenderer->damageMonitor(monitor);

    if (NEW_SCENE)
        m_events.opened.emit();
}

void COverview::close() {
    if (!m_isOpen || !m_sceneInstalled)
        return;

    m_isOpen = false;
    m_progress->setConfig(Config::animationTree()->getAnimationPropertyConfig("overviewOut"));
    *m_progress = 0.F;

    const auto MONITOR = m_monitor.lock();
    if (MONITOR && m_scene->selectedWorkspace() != MONITOR->m_activeWorkspace)
        Config::Actions::changeWorkspace(m_scene->selectedWorkspace());

    if (const auto MONITOR = m_monitor.lock(); MONITOR && g_pHyprRenderer)
        g_pHyprRenderer->damageMonitor(MONITOR);
}

bool COverview::isOpen() const {
    return m_isOpen;
}

bool COverview::shouldRenderWorkspace(PHLWORKSPACE workspace) const {
    return m_sceneInstalled && workspace && m_scene->selectedWorkspace() == workspace;
}

void COverview::finishClose(bool emitEvent) {
    if (m_isOpen || !m_sceneInstalled)
        return;

    m_sceneInstalled = false;
    m_listeners      = {};

    const auto MONITOR   = m_monitor.lock();
    const auto RESOURCES = m_resources;
    if (RESOURCES)
        RESOURCES->m_sceneStack.remove(m_scene);

    m_scene->reset();
    m_resources.reset();
    m_monitor.reset();

    if (MONITOR && g_pHyprRenderer) {
        MONITOR->recheckSolitary();
        g_pHyprRenderer->damageMonitor(MONITOR);
    }

    if (emitEvent)
        m_events.closed.emit();
}

void COverview::closeImmediately() {
    if (!m_sceneInstalled)
        return;

    m_isOpen = false;
    if (m_progress)
        m_progress->setValueAndWarp(0.F);
    finishClose();
}
