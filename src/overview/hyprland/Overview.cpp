#include "Overview.hpp"
#include "StringUtils.hpp"

#include "config/shared/actions/ConfigActions.hpp"
#include "desktop/state/FocusState.hpp"
#include "desktop/state/WindowState.hpp"
#include "desktop/view/window/Window.hpp"
#include "devices/IPointer.hpp"
#include "input/Keys.hpp"
#include "managers/eventLoop/EventLoopTimer.hpp"
#include "managers/input/InputManager.hpp"
#include "scene/OverviewScene.hpp"
#include "../../animation/AnimationManager.hpp"
#include "../../config/shared/animation/AnimationTree.hpp"
#include "../../event/EventBus.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"
#include "../../devices/IKeyboard.hpp"
#include "../../managers/SessionLockManager.hpp"
#include "../../output/Monitor.hpp"
#include "../../output/MonitorResources.hpp"
#include "../../pointer/PointerManager.hpp"
#include "../../pointer/PointerTransformer.hpp"
#include "../../protocols/core/DataDevice.hpp"
#include "../../render/Renderer.hpp"
#include "../../state/MonitorState.hpp"
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
            stopKeyRepeat(event.keycode);
            info.cancelled = true;
            return;
        }

        if (!m_isOpen || !keyboard || !keyboard->m_xkbState)
            return;

        if (!handleSearchKey(event.keycode, keyboard))
            return;

        info.cancelled = true;
        if (INTERCEPTED() == m_interceptedKeys.end())
            m_interceptedKeys.emplace_back(keyboard, event.keycode);
        if (m_isOpen)
            startKeyRepeat(event.keycode, keyboard);
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
        const auto INTERCEPTED = std::ranges::find(m_interceptedButtons, e.button);
        if (e.state == WL_POINTER_BUTTON_STATE_RELEASED && INTERCEPTED != m_interceptedButtons.end()) {
            if (const auto MONITOR = m_monitor.lock())
                m_scene->pointerButton(e.button, false, Pointer::mgr()->untransformedPosition() - MONITOR->logicalBox().pos());
            m_interceptedButtons.erase(INTERCEPTED);
            i.cancelled = true;
            return;
        }

        if (e.state != WL_POINTER_BUTTON_STATE_PRESSED)
            return;

        const auto MONITOR = m_monitor.lock();
        if (!MONITOR)
            return;

        const auto MOUSE = Pointer::mgr()->untransformedPosition();
        if (!MONITOR->logicalBox().containsPoint(MOUSE))
            return;

        if (m_scene->pointerButton(e.button, true, MOUSE - MONITOR->logicalBox().pos())) {
            m_interceptedButtons.emplace_back(e.button);
            i.cancelled = true;
            return;
        }

        // TODO: make this better. This is to support drags, and is obviously kinda wrong.
        if (g_pInputManager->getModsFromAllKBs() != Input::eKeyboardModifiers::HL_MODIFIER_NONE)
            return;

        close();

        i.cancelled = true;
    });

    m_listeners.mouseMove  = Event::bus()->m_events.input.mouse.move.listen([this](Vector2D, Event::SCallbackInfo& i) {
        const auto MONITOR = m_monitor.lock();
        if (!MONITOR)
            return;

        const auto RAW = Pointer::mgr()->untransformedPosition();
        if (!MONITOR->logicalBox().containsPoint(RAW)) {
            releaseDragFromOverview();
            return;
        }

        if (!g_layoutManager->dragController()->target())
            m_scene->pointerMove(RAW - MONITOR->logicalBox().pos());
        else
            g_layoutManager->moveMouse(g_pInputManager->getMouseCoordsInternal());

        i.cancelled = !PROTO::data || !PROTO::data->dndActive();
    });
    m_listeners.dragMotion = g_layoutManager->dragController()->m_events.motion.listen([this] { recheckDrag(); });
    m_listeners.dragEnded  = g_layoutManager->dragController()->m_events.ended.listen([this] { resetDragHover(); });
}

bool COverview::handleSearchKey(uint32_t keycode, SP<IKeyboard> keyboard, bool repeat) {
    if (!m_isOpen || !keyboard || !keyboard->m_xkbState)
        return false;

    const auto KEYSYM = xkb_state_key_get_one_sym(keyboard->m_xkbState, keycode + 8);

    // Text mode follows a small "vim"-like mode behavior:
    // By default, we're in NAVIGATE: escape closes, left/right navigates
    // if we start typing, we enter TEXT mode, and escape goes out of it, enter closes.
    // if we delete all the text with backspace, automatically goes back to navi.
    if (m_inputMode == eInputMode::NAVIGATION) {
        if (KEYSYM == XKB_KEY_Left) {
            m_scene->navigateLeft();
            return true;
        }

        if (KEYSYM == XKB_KEY_Right) {
            m_scene->navigateRight();
            return true;
        }

        if (KEYSYM == XKB_KEY_Escape) {
            close();
            return true;
        }
    } else if (m_inputMode == eInputMode::TEXT) {
        if (KEYSYM == XKB_KEY_Escape) {
            m_inputMode = eInputMode::NAVIGATION;
            m_scene->resetQuery();
            m_scene->setTextboxFocus(false);
            return true;
        }
    }

    if (KEYSYM == XKB_KEY_Return || KEYSYM == XKB_KEY_KP_Enter) {
        close();
        return true;
    }

    const int   UTF8_SIZE = xkb_state_key_get_utf8(keyboard->m_xkbState, keycode + 8, nullptr, 0);
    std::string utf8;
    if (UTF8_SIZE > 0) {
        utf8.resize(sc<size_t>(UTF8_SIZE) + 1);
        const int WRITTEN = xkb_state_key_get_utf8(keyboard->m_xkbState, keycode + 8, utf8.data(), utf8.size());
        utf8.resize(WRITTEN > 0 ? sc<size_t>(WRITTEN) : 0);
    }

    const auto MODIFIERS = keyboard->getModifiers();
    const bool EDIT_KEY  = KEYSYM == XKB_KEY_BackSpace || KEYSYM == XKB_KEY_Delete || KEYSYM == XKB_KEY_Left || KEYSYM == XKB_KEY_Right || KEYSYM == XKB_KEY_Home ||
        KEYSYM == XKB_KEY_End || KEYSYM == XKB_KEY_KP_Home || KEYSYM == XKB_KEY_KP_End;
    const bool TEXT_SHORTCUT         = (MODIFIERS & Input::HL_MODIFIER_CTRL) && (KEYSYM == XKB_KEY_a || KEYSYM == XKB_KEY_A);
    const bool BLOCKED_TEXT_MODIFIER = !!(MODIFIERS & (Input::HL_MODIFIER_CTRL | Input::HL_MODIFIER_ALT | Input::HL_MODIFIER_META));

    if (!EDIT_KEY && !TEXT_SHORTCUT && (utf8.empty() || BLOCKED_TEXT_MODIFIER))
        return false;

    m_scene->setTextboxFocus(true);
    m_scene->keyboardKey(KEYSYM, true, repeat, std::move(utf8), sc<uint32_t>(MODIFIERS));
    m_inputMode = eInputMode::TEXT;

    if (m_scene->currentQuery().empty()) {
        m_inputMode = eInputMode::NAVIGATION;
        m_scene->resetQuery();
        m_scene->setTextboxFocus(false);
        return true;
    }

    return true;
}

void COverview::startKeyRepeat(uint32_t keycode, SP<IKeyboard> keyboard) {
    if (!keyboard || !keyboard->m_xkbKeymap || keyboard->m_repeatRate <= 0 || !xkb_keymap_key_repeats(keyboard->m_xkbKeymap, keycode + 8)) {
        stopKeyRepeat(m_keyRepeat.keycode);
        return;
    }

    m_keyRepeat.keyboard = keyboard;
    m_keyRepeat.keycode  = keycode;
    if (!m_keyRepeat.timer) {
        m_keyRepeat.timer = makeShared<CEventLoopTimer>(
            std::nullopt,
            [this](SP<CEventLoopTimer> self, void*) {
                const auto KEYBOARD = m_keyRepeat.keyboard.lock();
                if (!m_isOpen || !KEYBOARD || KEYBOARD->m_repeatRate <= 0) {
                    self->updateTimeout(std::nullopt);
                    return;
                }

                if (!handleSearchKey(m_keyRepeat.keycode, KEYBOARD, true)) {
                    self->updateTimeout(std::nullopt);
                    return;
                }
                self->updateTimeout(std::chrono::milliseconds(std::max(1, 1000 / KEYBOARD->m_repeatRate)));
            },
            nullptr);
        g_pEventLoopManager->addTimer(m_keyRepeat.timer);
    }

    m_keyRepeat.timer->updateTimeout(std::chrono::milliseconds(std::max(0, keyboard->m_repeatDelay)));
}

void COverview::stopKeyRepeat(uint32_t keycode) {
    if (!m_keyRepeat.timer || m_keyRepeat.keycode != keycode)
        return;

    m_keyRepeat.timer->updateTimeout(std::nullopt);
    m_keyRepeat.keyboard.reset();
    m_keyRepeat.keycode = 0;
}

void COverview::recheckDrag() {
    const auto  MONITOR = m_monitor.lock();
    const auto& DRAG    = g_layoutManager->dragController();
    if (!m_isOpen || !MONITOR || !DRAG->target() || DRAG->mode() != MBIND_MOVE || !DRAG->dragThresholdReached()) {
        resetDragHover();
        return;
    }

    const auto MOUSE = Pointer::mgr()->untransformedPosition();
    if (!MONITOR->logicalBox().containsPoint(MOUSE)) {
        releaseDragFromOverview();
        return;
    }

    const auto MOUSE_LOCAL = MOUSE - MONITOR->logicalBox().pos();
    const auto MINI_TILE   = m_scene->miniWorkspaceAt(MOUSE_LOCAL);
    auto       target      = eDragHoverTarget::NONE;
    if (MINI_TILE)
        target = eDragHoverTarget::MINI_TILE;
    else {
        const auto MAIN_AREA = m_scene->mainArea();
        if (MOUSE_LOCAL.x < MAIN_AREA.x)
            target = eDragHoverTarget::LEFT_EDGE;
        else if (MOUSE_LOCAL.x > MAIN_AREA.x + MAIN_AREA.w)
            target = eDragHoverTarget::RIGHT_EDGE;
    }

    if (target == eDragHoverTarget::NONE) {
        resetDragHover();
        return;
    }

    if (target == m_drag.target && m_drag.dragTarget == DRAG->target() && (target != eDragHoverTarget::MINI_TILE || m_drag.workspace == MINI_TILE))
        return;

    m_drag.target     = target;
    m_drag.workspace  = MINI_TILE;
    m_drag.dragTarget = DRAG->target();
    if (!m_drag.eventLoopTimer) {
        m_drag.eventLoopTimer = makeShared<CEventLoopTimer>(std::nullopt, [this](SP<CEventLoopTimer>, void*) { applyDragHoverTarget(); }, nullptr);
        g_pEventLoopManager->addTimer(m_drag.eventLoopTimer);
    }

    m_drag.eventLoopTimer->updateTimeout(std::chrono::milliseconds(sc<int32_t>(DRAG_MOVE_MS)));
}

void COverview::applyDragHoverTarget() {
    const auto& DRAG    = g_layoutManager->dragController();
    const auto  MONITOR = m_monitor.lock();
    if (!m_isOpen || !MONITOR || !DRAG->target() || m_drag.dragTarget != DRAG->target() || DRAG->mode() != MBIND_MOVE || !DRAG->dragThresholdReached()) {
        resetDragHover();
        return;
    }

    const auto MOUSE = Pointer::mgr()->untransformedPosition();
    if (!MONITOR->logicalBox().containsPoint(MOUSE)) {
        releaseDragFromOverview();
        return;
    }

    const auto MOUSE_LOCAL   = MOUSE - MONITOR->logicalBox().pos();
    const auto MINI_TILE     = m_scene->miniWorkspaceAt(MOUSE_LOCAL);
    const auto MAIN_AREA     = m_scene->mainArea();
    const bool STILL_HOVERED = (m_drag.target == eDragHoverTarget::MINI_TILE && MINI_TILE && m_drag.workspace == MINI_TILE) ||
        (m_drag.target == eDragHoverTarget::LEFT_EDGE && !MINI_TILE && MOUSE_LOCAL.x < MAIN_AREA.x) ||
        (m_drag.target == eDragHoverTarget::RIGHT_EDGE && !MINI_TILE && MOUSE_LOCAL.x > MAIN_AREA.x + MAIN_AREA.w);
    if (!STILL_HOVERED) {
        resetDragHover();
        recheckDrag();
        return;
    }

    PHLWORKSPACE workspace;
    bool         repeat = false;
    switch (m_drag.target) {
        case eDragHoverTarget::LEFT_EDGE:
            if (m_scene->navigateLeft()) {
                workspace = m_scene->selectedWorkspace();
                repeat    = true;
            }
            break;
        case eDragHoverTarget::RIGHT_EDGE:
            if (m_scene->navigateRight()) {
                workspace = m_scene->selectedWorkspace();
                repeat    = true;
            }
            break;
        case eDragHoverTarget::MINI_TILE:
            workspace = m_drag.workspace.lock();
            if (workspace)
                m_scene->selectWorkspace(workspace);
            break;
        case eDragHoverTarget::NONE: return;
    }

    const auto TARGET = DRAG->target();
    if (!workspace || workspace != m_scene->selectedWorkspace() || !TARGET || !TARGET->window())
        return;

    TARGET->assignToSpace(workspace->m_space);
    DRAG->overrideDragWindowTargetWS(workspace);
    g_layoutManager->moveMouse(m_scene->transformPointer(Pointer::mgr()->untransformedPosition()));
    if (repeat && m_drag.eventLoopTimer)
        m_drag.eventLoopTimer->updateTimeout(std::chrono::milliseconds(sc<int32_t>(DRAG_MOVE_MS)));
}

void COverview::resetDragHover() {
    m_drag.target = eDragHoverTarget::NONE;
    m_drag.workspace.reset();
    m_drag.dragTarget.reset();
    if (m_drag.eventLoopTimer)
        m_drag.eventLoopTimer->updateTimeout(std::nullopt);
}

void COverview::releaseDragFromOverview() {
    const auto& DRAG = g_layoutManager->dragController();
    DRAG->clearDragWindowTargetWS();

    const auto TARGET = DRAG->target();
    if (!TARGET || DRAG->mode() != MBIND_MOVE || !DRAG->dragThresholdReached()) {
        resetDragHover();
        return;
    }

    const auto MONITOR   = State::monitorState()->query().vec(Pointer::mgr()->untransformedPosition()).run();
    const auto WORKSPACE = MONITOR ? (MONITOR->m_activeSpecialWorkspace ? MONITOR->m_activeSpecialWorkspace : MONITOR->m_activeWorkspace) : nullptr;
    if (TARGET->window() && WORKSPACE && TARGET->workspace() != WORKSPACE)
        TARGET->assignToSpace(WORKSPACE->m_space);

    resetDragHover();
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

    m_inputMode = eInputMode::NAVIGATION;
    m_scene->setTextboxFocus(false);

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
        m_sceneInstalled     = true;
        m_pointerTransformer = makeShared<Pointer::CPointerTransformer>(
            [this](Vector2D pos) { return m_sceneInstalled && (!PROTO::data || !PROTO::data->dndActive()) ? m_scene->transformPointer(pos) : pos; });
        Pointer::mgr()->addTransformer(m_pointerTransformer);
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
    resetDragHover();
    m_progress->setConfig(Config::animationTree()->getAnimationPropertyConfig("overviewOut"));
    *m_progress = 0.F;

    const auto MONITOR  = m_monitor.lock();
    const auto SELECTED = m_scene->selectedWorkspace();
    if (MONITOR && SELECTED && SELECTED != MONITOR->m_activeWorkspace)
        Config::Actions::changeWorkspace(SELECTED);

    const auto QUERY = m_scene->currentQuery();

    if (!QUERY.empty()) {
        // select the window, if applicable
        // if we match the workspace name our search is exclusive for the workspace, don't focus shit
        if (!StringUtils::fullMatchCaseIns(MONITOR->m_activeWorkspace->m_name, QUERY)) {
            for (const auto& w : Desktop::windowState()->windows()) {
                if (w->m_workspace != MONITOR->m_activeWorkspace || !w->focusAvailable())
                    continue;

                if (!StringUtils::matchesName(w->metadata().appID(), QUERY) && !StringUtils::matchesName(w->metadata().title(), QUERY))
                    continue;

                Desktop::focusState()->fullWindowFocus(w, Desktop::eFocusReason::FOCUS_REASON_SWITCH_TO_WINDOW_HARD);
                break;
            }
        }
    }

    if (const auto MONITOR = m_monitor.lock(); MONITOR && g_pHyprRenderer)
        g_pHyprRenderer->damageMonitor(MONITOR);
}

bool COverview::isOpen() const {
    return m_isOpen;
}

bool COverview::shouldRenderWorkspace(PHLWORKSPACE workspace) const {
    return m_sceneInstalled && workspace && workspace->m_visible && m_scene->selectedWorkspace() == workspace;
}

PHLWORKSPACE COverview::inputWorkspace() const {
    const auto MONITOR = m_monitor.lock();
    if (!m_sceneInstalled || !MONITOR || !Pointer::mgr() || !MONITOR->logicalBox().containsPoint(Pointer::mgr()->untransformedPosition()))
        return nullptr;

    return m_scene->selectedWorkspace();
}

void COverview::finishClose(bool emitEvent) {
    if (m_isOpen || !m_sceneInstalled)
        return;

    m_sceneInstalled = false;
    resetDragHover();
    stopKeyRepeat(m_keyRepeat.keycode);
    m_listeners = {};
    m_interceptedKeys.clear();
    m_interceptedButtons.clear();

    if (g_layoutManager)
        g_layoutManager->dragController()->clearDragWindowTargetWS();
    if (Pointer::mgr())
        Pointer::mgr()->removeTransformer(m_pointerTransformer);
    m_pointerTransformer.reset();

    const auto MONITOR   = m_monitor.lock();
    const auto RESOURCES = m_resources;
    if (RESOURCES)
        RESOURCES->m_sceneStack.remove(m_scene);

    m_scene->reset();
    m_resources.reset();
    m_monitor.reset();

    if (g_pInputManager)
        g_pInputManager->simulateMouseMovement();

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
    resetDragHover();
    if (m_progress)
        m_progress->setValueAndWarp(0.F);
    finishClose();
}

SP<COverviewScene> COverview::scene() const {
    return m_scene;
}
