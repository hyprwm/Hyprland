#include "../private.hpp"
#include "../globals.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>
using namespace Hyprutils::Utils;

static SDispatchResult testPinchDeltaScale(lua_State* L) {
    const auto                          SCALE           = (float)luaL_checknumber(L, 1);
    constexpr size_t                    FINGERS         = 42;
    constexpr eTrackpadGestureDirection DIRECTION       = TRACKPAD_GESTURE_DIR_PINCH;
    constexpr bool                      DISABLE_INHIBIT = true;

    if (g_pTrackpadGestures->m_activeGesture)
        return {.success = false, .error = "A trackpad gesture is already active"};

    const auto  OLD_MODS = g_pInputManager->m_lastMods;
    CScopeGuard RESTORE_MODS([OLD_MODS] { g_pInputManager->m_lastMods = OLD_MODS; });
    g_pInputManager->m_lastMods = Input::HL_MODIFIER_NONE;

    const auto EVENTS = makeShared<SPinchScaleEvents>();
    const auto ADDED  = g_pTrackpadGestures->addGesture(makeUnique<CPinchScaleRecorder>(EVENTS), FINGERS, DIRECTION, Input::HL_MODIFIER_NONE, SCALE, DISABLE_INHIBIT);
    if (!ADDED)
        return {.success = false, .error = ADDED.error()};

    g_pTrackpadGestures->gestureBegin(IPointer::SPinchBeginEvent{.fingers = FINGERS});
    g_pTrackpadGestures->gestureUpdate(IPointer::SPinchUpdateEvent{.fingers = FINGERS, .scale = 1.2});
    g_pTrackpadGestures->gestureUpdate(IPointer::SPinchUpdateEvent{.fingers = FINGERS, .scale = 1.4});
    g_pTrackpadGestures->gestureEnd(IPointer::SPinchEndEvent{.cancelled = true});

    const auto REMOVED = g_pTrackpadGestures->removeGesture(FINGERS, DIRECTION, Input::HL_MODIFIER_NONE, SCALE, DISABLE_INHIBIT);
    if (!REMOVED)
        return {.success = false, .error = REMOVED.error()};

    const auto SCALE_MATCHES = [SCALE](float actual) { return std::abs(actual - SCALE) < 0.001F; };
    if (EVENTS->beginCount != 1 || EVENTS->updateScales.size() != 2 || EVENTS->endCount != 1)
        return {.success = false,
                .error   = std::format("Unexpected pinch callback counts: begin {}, update {}, end {}", EVENTS->beginCount, EVENTS->updateScales.size(), EVENTS->endCount)};
    if (!SCALE_MATCHES(EVENTS->beginScale) || !std::ranges::all_of(EVENTS->updateScales, SCALE_MATCHES) || !SCALE_MATCHES(EVENTS->endScale))
        return {.success = false, .error = std::format("Configured pinch scale {} was not propagated to every callback", SCALE)};

    return {};
}

static SDispatchResult simulateGesture(lua_State* L) {
    const auto DIRECTION = std::string{luaL_checkstring(L, 1)};
    const auto FINGERS   = sc<uint32_t>(luaL_optinteger(L, 2, 3));

    if (DIRECTION == "down") {
        g_pTrackpadGestures->gestureBegin(IPointer::SSwipeBeginEvent{});
        g_pTrackpadGestures->gestureUpdate(IPointer::SSwipeUpdateEvent{.fingers = FINGERS, .delta = {0, 300}});
        g_pTrackpadGestures->gestureEnd(IPointer::SSwipeEndEvent{});
    } else if (DIRECTION == "up") {
        g_pTrackpadGestures->gestureBegin(IPointer::SSwipeBeginEvent{});
        g_pTrackpadGestures->gestureUpdate(IPointer::SSwipeUpdateEvent{.fingers = FINGERS, .delta = {0, -300}});
        g_pTrackpadGestures->gestureEnd(IPointer::SSwipeEndEvent{});
    } else if (DIRECTION == "left") {
        g_pTrackpadGestures->gestureBegin(IPointer::SSwipeBeginEvent{});
        g_pTrackpadGestures->gestureUpdate(IPointer::SSwipeUpdateEvent{.fingers = FINGERS, .delta = {-300, 0}});
        g_pTrackpadGestures->gestureEnd(IPointer::SSwipeEndEvent{});
    } else {
        g_pTrackpadGestures->gestureBegin(IPointer::SSwipeBeginEvent{});
        g_pTrackpadGestures->gestureUpdate(IPointer::SSwipeUpdateEvent{.fingers = FINGERS, .delta = {300, 0}});
        g_pTrackpadGestures->gestureEnd(IPointer::SSwipeEndEvent{});
    }

    return {.success = true};
}

static SDispatchResult pinchUpdate(lua_State* L) {
    const auto FINGERS  = sc<uint32_t>(luaL_checkinteger(L, 1));
    const auto SCALE    = (double)luaL_checknumber(L, 2);
    const auto DELTA    = Vector2D{luaL_optnumber(L, 3, 0), luaL_optnumber(L, 4, 0)};
    const auto ROTATION = (double)luaL_optnumber(L, 5, 0);

    g_pTrackpadGestures->gestureUpdate(IPointer::SPinchUpdateEvent{
        .fingers  = FINGERS,
        .delta    = DELTA,
        .scale    = SCALE,
        .rotation = ROTATION,
    });

    return {};
}

static SDispatchResult pinchEnd(lua_State* L) {
    g_pTrackpadGestures->gestureEnd(IPointer::SPinchEndEvent{});

    return {};
}

static SDispatchResult expectCursorZoom(lua_State* L) {
    const auto EXPECTED = (float)luaL_checknumber(L, 1);
    const auto DELTA    = (float)luaL_optnumber(L, 2, 0.01);

    const auto PMONITOR = State::monitorState()->query().vec(Pointer::mgr()->untransformedPosition()).run();

    if (!PMONITOR)
        return {.success = false, .error = "No monitor under cursor"};

    const auto actual = PMONITOR->m_cursorZoom->value();

    if (std::abs(actual - EXPECTED) > DELTA)
        return {.success = false, .error = std::format("Expected cursor zoom {} ± {}, got {}", EXPECTED, DELTA, actual)};

    return {};
}

REGISTER_UNIT(gestures) {
    registerLuaFn<testPinchDeltaScale>("test_pinch_delta_scale");
    registerLuaFn<simulateGesture>("gesture");
    registerLuaFn<pinchUpdate>("pinch_update");
    registerLuaFn<pinchEnd>("pinch_end");
    registerLuaFn<expectCursorZoom>("expect_cursor_zoom");
}