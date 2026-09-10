#include "../private.hpp"
#include "../globals.hpp"

static PHLWINDOW windowByClass(const std::string& cls) {
    for (const auto& window : Desktop::windowState()->windows()) {
        if (window->metadata().appID() == cls)
            return window;
    }

    return nullptr;
}

// Trigger a snap move event for the active window
static SDispatchResult snapMove(lua_State* L) {
    const auto PLASTWINDOW = Desktop::focusState()->window();
    if (!PLASTWINDOW->isFloating())
        return {.success = false, .error = "Window must be floating"};

    Vector2D pos  = PLASTWINDOW->position(Desktop::View::IGeometric::GEOMETRIC_GOAL);
    Vector2D size = PLASTWINDOW->size(Desktop::View::IGeometric::GEOMETRIC_GOAL);

    g_layoutManager->performSnap(pos, size, PLASTWINDOW->layoutTarget(), MBIND_MOVE, -1, size);

    PLASTWINDOW->layoutTarget()->setPositionGlobal(CBox{pos, size});

    return {};
}

static SDispatchResult expectWindowAtWorkspace(lua_State* L) {
    const auto WORKSPACESELECTOR = std::string{luaL_checkstring(L, 1)};
    const auto POS               = Vector2D{luaL_checknumber(L, 2), luaL_checknumber(L, 3)};
    const auto EXPECTEDCLASS     = std::string{luaL_checkstring(L, 4)};
    const auto IGNORECLASS       = lua_gettop(L) > 4 ? std::string{luaL_checkstring(L, 5)} : std::string{};

    const auto WORKSPACE = State::Workspace::state()->find(State::Workspace::resolver()->getWorkspaceTargetFromString(WORKSPACESELECTOR));
    if (!WORKSPACE)
        return {.success = false, .error = std::format("No workspace matching '{}'", WORKSPACESELECTOR)};

    const auto IGNORE = IGNORECLASS.empty() ? nullptr : windowByClass(IGNORECLASS);
    if (!IGNORECLASS.empty() && !IGNORE)
        return {.success = false, .error = std::format("No window with class '{}' to ignore", IGNORECLASS)};

    const auto WINDOW =
        Desktop::viewState()->hitTest().windowAtWorkspace(POS, WORKSPACE, Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING, IGNORE);

    if (!WINDOW)
        return {.success = false, .error = std::format("Expected window '{}', got no window", EXPECTEDCLASS)};
    if (WINDOW->metadata().appID() != EXPECTEDCLASS)
        return {.success = false, .error = std::format("Expected window '{}', got '{}'", EXPECTEDCLASS, WINDOW->metadata().appID())};

    return {};
}

static SDispatchResult testDragLifecycle(lua_State* L) {
    const auto CLS    = std::string{luaL_checkstring(L, 1)};
    const auto WINDOW = windowByClass(CLS);
    if (!WINDOW)
        return {.success = false, .error = std::format("No window with class '{}'", CLS)};

    const auto TARGET = WINDOW->layoutTarget();
    if (!TARGET)
        return {.success = false, .error = "Window has no layout target"};

    const auto& CONTROLLER = g_layoutManager->dragController();
    if (CONTROLLER->target())
        return {.success = false, .error = "A drag is already active"};

    std::vector<std::string> events;
    bool                     motionHadTarget = false;
    bool                     endedWasReset   = false;
    const auto               MOTION_LISTENER = CONTROLLER->m_events.motion.listen([&] {
        events.emplace_back("motion");
        motionHadTarget = !!CONTROLLER->target();
    });
    const auto               ENDED_LISTENER  = CONTROLLER->m_events.ended.listen([&] {
        events.emplace_back("ended");
        endedWasReset = !CONTROLLER->target() && CONTROLLER->mode() == MBIND_INVALID;
    });

    const auto               START = TARGET->position().middle();
    Pointer::pointerController()->warpTo(START, true);
    g_layoutManager->beginDragTarget(TARGET, MBIND_MOVE);
    g_layoutManager->moveMouse(START + Vector2D{100, 100});
    const bool ENDED       = g_layoutManager->endDragTarget();
    const bool ENDED_AGAIN = g_layoutManager->endDragTarget();

    if (!ENDED || ENDED_AGAIN)
        return {.success = false, .error = std::format("Unexpected drag end results: first {}, second {}", ENDED, ENDED_AGAIN)};
    if (events != std::vector<std::string>{"motion", "ended"})
        return {.success = false, .error = std::format("Expected one motion and one ended event, got {} total events", events.size())};
    if (!motionHadTarget)
        return {.success = false, .error = "Drag motion event fired without an active target"};
    if (!endedWasReset)
        return {.success = false, .error = "Drag ended event fired before state was reset"};

    return {};
}

// Perform a full move-drag of the window with the given class and drop it at (x, y).
static SDispatchResult dragWindow(lua_State* L) {
    const auto CLS = std::string{luaL_checkstring(L, 1)};
    const auto X   = (double)luaL_checknumber(L, 2);
    const auto Y   = (double)luaL_checknumber(L, 3);

    for (const auto& window : Desktop::windowState()->windows()) {
        if (window->metadata().appID() != CLS)
            continue;

        const auto target = window->layoutTarget();
        if (!target)
            return {.success = false, .error = "Window has no layout target"};

        Pointer::pointerController()->warpTo({X, Y}, true);
        g_layoutManager->beginDragTarget(target, MBIND_MOVE);
        g_layoutManager->endDragTarget();

        return {};
    }

    return {.success = false, .error = std::format("No window with class '{}'", CLS)};
}

static SDispatchResult softFocusWindowByClass(lua_State* L) {
    const auto CLS = std::string{luaL_checkstring(L, 1)};

    for (const auto& window : Desktop::windowState()->windows()) {
        if (window->metadata().appID() != CLS)
            continue;

        Desktop::focusState()->rawWindowFocus(window, Desktop::FOCUS_REASON_FFM);
        return {};
    }

    return {.success = false, .error = std::format("No window with class '{}'", CLS)};
}

static SDispatchResult floatingFocusOnFullscreen(lua_State* L) {
    const auto PLASTWINDOW = Desktop::focusState()->window();

    if (!PLASTWINDOW)
        return {.success = false, .error = "No window"};

    if (!PLASTWINDOW->isFloating())
        return {.success = false, .error = "Window must be floating"};

    if (PLASTWINDOW->presentation().alphaTotalGoal() != 1.F)
        return {.success = false, .error = "floating window doesnt restore it opacity when focused on fullscreen workspace"};

    if (!PLASTWINDOW->fullscreenPolicy().allowedOverFullscreen())
        return {.success = false, .error = "floating window doesnt get flagged as allowedOverFullscreen"};

    return {};
}

static SDispatchResult expectNoMaximizeEcho(lua_State* L) {
    const auto WINDOW = Desktop::focusState()->window();
    if (!WINDOW)
        return {.success = false, .error = "No window"};

    if (WINDOW->fullscreenPolicy().consumeExpectedMaximizeEcho(true))
        return {.success = false, .error = "Window has a stale maximize echo expectation"};

    return {};
}

REGISTER_UNIT(windows) {
    registerLuaFn<snapMove>("snapmove");
    registerLuaFn<dragWindow>("drag_window");
    registerLuaFn<expectWindowAtWorkspace>("expect_window_at_workspace");
    registerLuaFn<testDragLifecycle>("test_drag_lifecycle");
    registerLuaFn<softFocusWindowByClass>("window_soft_focus");
    registerLuaFn<floatingFocusOnFullscreen>("floating_focus_on_fullscreen");
    registerLuaFn<expectNoMaximizeEcho>("expect_no_maximize_echo");
}