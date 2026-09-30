#include "WorkspaceGestures.hpp"
#include "globals.hpp"

#include <src/animation/AnimationManager.hpp>
#include <src/config/ConfigValue.hpp>
#include <src/desktop/state/FocusState.hpp>
#include <src/devices/ITouch.hpp>
#include <src/event/EventBus.hpp>
#include <src/managers/input/InputManager.hpp>
#include <src/managers/input/UnifiedWorkspaceSwipeGesture.hpp>
#include <src/managers/input/trackpad/gestures/WorkspaceSwipeGesture.hpp>
#include <src/managers/input/trackpad/TrackpadGestures.hpp>
#include <src/managers/SeatManager.hpp>
#include <src/output/Monitor.hpp>
#include <src/pointer/PointerManager.hpp>
#include <src/state/MonitorState.hpp>
#include <src/state/workspace/Resolver.hpp>
#include <src/state/workspace/State.hpp>
#include <src/workspace/HLWorkspace.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

#include <array>
#include <cmath>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace WorkspaceGestures {
    class CTestTouch : public ITouch {
      public:
        bool                   isVirtual() const override;
        SP<Aquamarine::ITouch> aq() override;
    };

    bool CTestTouch::isVirtual() const {
        return true;
    }

    SP<Aquamarine::ITouch> CTestTouch::aq() {
        return nullptr;
    }

    // Holds keep empty fixture workspaces alive between staged Lua calls.
    static std::array<PHLWORKSPACE, 3>         g_workspaces;
    static PHLWORKSPACE                        g_previous;
    static PHLMONITORREF                       g_monitor;
    static bool                                g_swiping = false;
    static uint32_t                            g_fingers = 0;
    static PHLWORKSPACE                        g_peerPrevious;
    static PHLWORKSPACE                        g_peerWorkspace;
    static std::array<SP<ITouch>, 2>           g_touches;
    static std::array<std::vector<int32_t>, 2> g_contacts;
    static CHyprSignalListener                 g_touchFocusListener;
    static size_t                              g_touchFocusEvents = 0;

    static void                                restorePrevious(PHLWORKSPACE previous) {
        // Output removal migrates workspaces; their current owner, not the captured output,
        // must receive the restoration. Never reactivate a workspace on a disabled monitor.
        const auto MONITOR = previous ? previous->m_monitor.lock() : nullptr;
        if (MONITOR && MONITOR->m_enabled)
            MONITOR->changeWorkspace(previous);
    }

    static void resetTouches() {
        for (size_t i = 0; i < g_touches.size(); ++i) {
            const auto& TOUCH = g_touches[i];
            if (!TOUCH)
                continue;
            for (const auto ID : g_contacts[i])
                TOUCH->m_touchEvents.up.emit(ITouch::SUpEvent{.touchID = ID});
            TOUCH->m_events.destroy.emit();
            Pointer::mgr()->detachTouch(TOUCH);
            std::erase_if(g_pInputManager->m_touchData.consumedTouches, [&](const auto& contact) { return contact.device == TOUCH; });
        }
        g_contacts = {};
        g_touches  = {};
        g_touchFocusListener.reset();
        g_touchFocusEvents = 0;
    }

    void reset() {
        if (g_previous) {
            g_pUnifiedWorkspaceSwipe->cancel();
            resetTouches();
            if (g_swiping)
                g_pTrackpadGestures->gestureEnd(IPointer::SSwipeEndEvent{.cancelled = true});

            restorePrevious(g_peerPrevious);
            restorePrevious(g_previous);

            for (const auto& workspace : g_workspaces) {
                if (!workspace)
                    continue;
                workspace->m_forceRendering = false;
                workspace->m_alpha->setValueAndWarp(0.F);
                workspace->m_renderOffset->setValueAndWarp({});
            }
            Animation::mgr()->tick();
        }

        g_swiping    = false;
        g_fingers    = 0;
        g_workspaces = {};
        g_previous.reset();
        g_monitor.reset();
        g_peerWorkspace.reset();
        g_peerPrevious.reset();
    }

    static std::string prepareFixture(bool bounds, bool forced) {
        reset();
        const auto MONITOR = Desktop::focusState()->monitor();
        if (!MONITOR || !MONITOR->m_activeWorkspace || g_pUnifiedWorkspaceSwipe->isGestureInProgress())
            return "Workspace swipe fixture requires an idle active monitor";
        if (*CConfigValue<Config::INTEGER>("animations:enabled"))
            return "Disable animations before preparing the workspace swipe fixture";

        g_monitor                           = MONITOR;
        g_previous                          = MONITOR->m_activeWorkspace;
        bool                          ready = false;
        Hyprutils::Utils::CScopeGuard rollback([&] {
            if (!ready)
                reset();
        });

        // Do not parse the UINT32_MAX literal: user-facing numeric selectors are int32-limited.
        g_workspaces[0] = State::Workspace::state()->createNumbered(Workspace::SWorkspaceNumberedID{bounds ? UINT32_MAX - 1 : 9901}, MONITOR);
        g_workspaces[1] = State::Workspace::state()->createNumbered(Workspace::SWorkspaceNumberedID{bounds ? UINT32_MAX : 9902}, MONITOR);
        g_workspaces[2] = bounds ? State::Workspace::state()->createNamed("hyprtester-swipe-wrap", MONITOR) :
                                   State::Workspace::state()->createNumbered(Workspace::SWorkspaceNumberedID{9903}, MONITOR);
        for (const auto& workspace : g_workspaces) {
            if (!workspace || workspace->m_monitor != MONITOR)
                return "Could not create workspace swipe fixture on the active monitor";
        }

        MONITOR->changeWorkspace(g_workspaces[1]);
        Animation::mgr()->tick();

        // Distinct non-neutral baselines catch cleanup that merely zeroes previews.
        g_workspaces[0]->m_alpha->setValueAndWarp(0.25F);
        g_workspaces[0]->m_renderOffset->setValueAndWarp({31, -17});
        g_workspaces[2]->m_alpha->setValueAndWarp(0.625F);
        g_workspaces[2]->m_renderOffset->setValueAndWarp({-29, 11});
        for (const auto& workspace : g_workspaces)
            workspace->m_forceRendering = forced;

        if (bounds) {
            const auto LEFT  = State::Workspace::resolver()->getWorkspaceTargetFromString("m-1");
            const auto RIGHT = State::Workspace::resolver()->getWorkspaceTargetFromString("m+1");
            if (State::Workspace::state()->find(LEFT) != g_workspaces[0] || State::Workspace::state()->find(RIGHT) != g_workspaces[2] ||
                State::Workspace::resolver()->getWorkspaceTargetFromString("r+1").valid())
                return "Bounds fixture did not establish valid m neighbors and an invalid r+1 target";
        }

        ready = true;
        return {};
    }

    static int luaFixture(lua_State* L) {
        luaL_checktype(L, 1, LUA_TBOOLEAN);
        luaL_checktype(L, 2, LUA_TBOOLEAN);
        // Finish all C++ cleanup before lua_error's longjmp.
        bool failed = false;
        {
            const auto ERROR = prepareFixture(lua_toboolean(L, 1), lua_toboolean(L, 2));
            failed           = !ERROR.empty();
            if (failed)
                lua_pushstring(L, ERROR.c_str());
        }
        return failed ? lua_error(L) : 0;
    }

    static int luaReset(lua_State* L) {
        reset();
        return 0;
    }

    static int luaBegin(lua_State* L) {
        const auto FINGERS = luaL_checkinteger(L, 1);
        if (!g_previous || g_swiping || FINGERS <= 0 || FINGERS > UINT32_MAX)
            return luaL_error(L, "Prepare a fixture and end the previous swipe before beginning");
        g_fingers = sc<uint32_t>(FINGERS);
        g_swiping = true;
        g_pTrackpadGestures->gestureBegin(IPointer::SSwipeBeginEvent{.fingers = g_fingers});
        return 0;
    }

    static int luaUpdate(lua_State* L) {
        const auto DELTA = luaL_checknumber(L, 1);
        if (!g_swiping || !std::isfinite(DELTA))
            return luaL_error(L, "Expected an active staged swipe and a finite delta");
        // Backend deltas are incremental; the real workspace gesture accumulates them.
        g_pTrackpadGestures->gestureUpdate(IPointer::SSwipeUpdateEvent{.fingers = g_fingers, .delta = {DELTA, 0.0}});
        return 0;
    }

    static int luaEnd(lua_State* L) {
        luaL_checktype(L, 1, LUA_TBOOLEAN);
        if (!g_swiping)
            return luaL_error(L, "No staged swipe to end");
        g_pTrackpadGestures->gestureEnd(IPointer::SSwipeEndEvent{.cancelled = sc<bool>(lua_toboolean(L, 1))});
        g_swiping = false;
        return 0;
    }

    static int luaCancel(lua_State* L) {
        g_pUnifiedWorkspaceSwipe->cancel();
        return 0;
    }

    static int luaPeer(lua_State* L) {
        const auto NAME  = luaL_checkstring(L, 1);
        bool       ready = false;
        {
            const auto MONITOR = State::monitorState()->query().name(NAME).run();
            if (g_previous && !g_peerPrevious && MONITOR && MONITOR->m_enabled && MONITOR != g_monitor && MONITOR->m_activeWorkspace) {
                g_peerPrevious  = MONITOR->m_activeWorkspace;
                g_peerWorkspace = State::Workspace::state()->createNumbered(Workspace::SWorkspaceNumberedID{9910}, MONITOR);
                if (g_peerWorkspace && g_peerWorkspace->m_monitor == MONITOR) {
                    MONITOR->changeWorkspace(g_peerWorkspace);
                    ready = true;
                }
            }
        }
        return ready ? 0 : luaL_error(L, "Could not prepare the peer output workspace");
    }

    static int luaRemoveOriginOutput(lua_State* L) {
        bool removed = false;
        {
            const auto MONITOR = g_monitor.lock();
            if (MONITOR && MONITOR->m_name == "HYPRTEST-SWIPE-A")
                removed = HyprlandAPI::invokeHyprctlCommand("output", "remove " + MONITOR->m_name) == "ok";
        }
        return removed ? 0 : luaL_error(L, "Could not remove the fixture output");
    }

    static std::string probeAdapter(bool stale) {
        auto       adapter = makeUnique<CWorkspaceSwipeGesture>();
        const auto MONITOR = Desktop::focusState()->monitor();
        if (!g_previous || !MONITOR || g_pUnifiedWorkspaceSwipe->isGestureInProgress())
            return "Adapter probe requires an idle fixture";
        Hyprutils::Utils::CScopeGuard                 cleanup([] { g_pUnifiedWorkspaceSwipe->cancel(); });
        const IPointer::SSwipeUpdateEvent             UPDATE{.fingers = 9, .delta = {180, 0}};
        const ITrackpadGesture::STrackpadGestureBegin BEGIN{.swipe = &UPDATE, .direction = TRACKPAD_GESTURE_DIR_HORIZONTAL};
        if (stale) {
            adapter->begin(BEGIN);
            if (!g_pUnifiedWorkspaceSwipe->sessionID())
                return "Adapter did not start its original session";
            g_pUnifiedWorkspaceSwipe->cancel();
        }
        if (!g_pUnifiedWorkspaceSwipe->begin(MONITOR))
            return "Could not start the independent session";
        g_pUnifiedWorkspaceSwipe->update(-90);
        const auto SESSION = g_pUnifiedWorkspaceSwipe->sessionID();
        const auto OFFSET  = MONITOR->m_activeWorkspace->m_renderOffset->goal();
        if (!stale)
            adapter->begin(BEGIN); // Refused: another owner already has the unified gesture.
        adapter->update({.swipe = &UPDATE, .direction = TRACKPAD_GESTURE_DIR_HORIZONTAL});
        if (g_pUnifiedWorkspaceSwipe->sessionID() != SESSION || MONITOR->m_activeWorkspace->m_renderOffset->goal() != OFFSET)
            return "Rejected/stale adapter updated another owner's session";
        adapter->end({});
        if (g_pUnifiedWorkspaceSwipe->sessionID() != SESSION)
            return "Rejected/stale adapter ended another owner's session";
        adapter.reset();
        if (g_pUnifiedWorkspaceSwipe->sessionID() != SESSION)
            return "Rejected/stale adapter destructor cancelled another owner's session";
        return {};
    }

    static int luaProbeAdapter(lua_State* L) {
        luaL_checktype(L, 1, LUA_TBOOLEAN);
        bool failed = false;
        {
            const auto ERROR = probeAdapter(lua_toboolean(L, 1));
            failed           = !ERROR.empty();
            if (failed)
                lua_pushstring(L, ERROR.c_str());
        }
        return failed ? lua_error(L) : 0;
    }

    static std::string probeReentrantEnd(int direction, bool redirect) {
        const auto MONITOR = g_monitor.lock();
        if (!g_previous || !MONITOR || MONITOR->m_activeWorkspace != g_workspaces[1] || g_pUnifiedWorkspaceSwipe->isGestureInProgress())
            return "Reentrant end probe requires an idle origin fixture";
        if (!g_pUnifiedWorkspaceSwipe->begin(MONITOR))
            return "Could not start the original session";

        const auto ORIGINAL_SESSION   = g_pUnifiedWorkspaceSwipe->sessionID();
        const auto DESTINATION        = g_workspaces[direction < 0 ? 0 : 2];
        const auto REPLACEMENT        = redirect ? g_workspaces[1] : DESTINATION;
        const auto NEIGHBOR           = redirect ? DESTINATION : g_workspaces[1];
        uint64_t   replacementSession = 0;
        Vector2D   replacementOffset, neighborOffset;
        bool       called   = false;
        auto       listener = Event::bus()->m_events.workspace.active.listen([&](PHLWORKSPACE workspace) {
            if (called || workspace != DESTINATION)
                return;
            called = true;
            g_pUnifiedWorkspaceSwipe->cancel();
            if (redirect)
                MONITOR->changeWorkspace(REPLACEMENT);
            if (!g_pUnifiedWorkspaceSwipe->begin(MONITOR))
                return;
            g_pUnifiedWorkspaceSwipe->update((redirect ? direction : -direction) * 90);
            replacementSession = g_pUnifiedWorkspaceSwipe->sessionID();
            replacementOffset  = REPLACEMENT->m_renderOffset->goal();
            neighborOffset     = NEIGHBOR->m_renderOffset->goal();
        });

        const bool FOREVER = *CConfigValue<Config::INTEGER>("gestures:workspace_swipe_forever");
        g_pUnifiedWorkspaceSwipe->update(direction * (FOREVER ? 300 : 180));
        if (!FOREVER)
            g_pUnifiedWorkspaceSwipe->end();

        if (!called || !replacementSession || replacementSession == ORIGINAL_SESSION)
            return "Activation callback did not establish a replacement session";
        if (g_pUnifiedWorkspaceSwipe->sessionID() != replacementSession)
            return "Outer end replaced or cancelled the reentrant session";
        if (REPLACEMENT->m_renderOffset->value() != replacementOffset || REPLACEMENT->m_renderOffset->goal() != replacementOffset ||
            NEIGHBOR->m_renderOffset->value() != neighborOffset || NEIGHBOR->m_renderOffset->goal() != neighborOffset)
            return "Outer end overwrote replacement preview offsets";
        if (!REPLACEMENT->m_forceRendering || !NEIGHBOR->m_forceRendering || NEIGHBOR->m_alpha->value() != 1.F || NEIGHBOR->m_alpha->goal() != 1.F)
            return "Outer end released replacement preview holds or alpha";
        return {};
    }

    static int luaProbeReentrantEnd(lua_State* L) {
        const auto DIRECTION = luaL_checkinteger(L, 1);
        luaL_checktype(L, 2, LUA_TBOOLEAN);
        if (DIRECTION != -1 && DIRECTION != 1)
            return luaL_error(L, "Expected swipe direction -1 or 1");
        bool failed = false;
        {
            const auto ERROR = probeReentrantEnd(sc<int>(DIRECTION), lua_toboolean(L, 2));
            failed           = !ERROR.empty();
            if (failed)
                lua_pushstring(L, ERROR.c_str());
        }
        return failed ? lua_error(L) : 0;
    }

    static int luaTouchSetup(lua_State* L) {
        if (!g_previous || !g_monitor || g_touches[0] || !g_pInputManager->m_touchData.consumedTouches.empty() || g_pInputManager->m_touchData.workspaceSwipe)
            return luaL_error(L, "Touch setup requires an idle fixture and no outstanding gesture contacts");
        g_touchFocusEvents   = 0;
        g_touchFocusListener = g_pSeatManager->m_events.touchFocusChange.listen([] { ++g_touchFocusEvents; });
        for (size_t i = 0; i < g_touches.size(); ++i) {
            auto& touch          = g_touches[i];
            touch                = makeShared<CTestTouch>();
            touch->m_self        = touch;
            touch->m_hlName      = std::format("hyprtester-swipe-touch-{}", i);
            touch->m_boundOutput = g_monitor->m_name;
            Pointer::mgr()->attachTouch(touch);
        }
        return 0;
    }

    static int luaTouch(lua_State* L) {
        const auto ACTION = std::string_view{luaL_checkstring(L, 1)};
        const auto DEVICE = luaL_checkinteger(L, 2);
        const auto ID     = luaL_checkinteger(L, 3);
        const auto X      = luaL_optnumber(L, 4, 0.5);
        if (DEVICE < 0 || DEVICE >= sc<lua_Integer>(g_touches.size()) || !g_touches[DEVICE] || ID < 0 || ID > INT32_MAX || !std::isfinite(X))
            return luaL_error(L, "Invalid synthetic touch event");
        auto&      touch   = g_touches[DEVICE];
        const auto CONTACT = sc<int32_t>(ID);
        if (ACTION == "down") {
            g_contacts[DEVICE].push_back(CONTACT);
            touch->m_touchEvents.down.emit(ITouch::SDownEvent{.touchID = CONTACT, .pos = {X, 0.5}, .device = touch});
        } else if (ACTION == "motion")
            touch->m_touchEvents.motion.emit(ITouch::SMotionEvent{.touchID = CONTACT, .pos = {X, 0.5}});
        else if (ACTION == "up") {
            touch->m_touchEvents.up.emit(ITouch::SUpEvent{.touchID = CONTACT});
            std::erase(g_contacts[DEVICE], CONTACT);
        } else if (ACTION == "cancel") {
            const bool CONSUMED =
                std::ranges::any_of(g_pInputManager->m_touchData.consumedTouches, [&](const auto& contact) { return contact.device == touch && contact.id == CONTACT; });
            touch->m_touchEvents.cancel.emit(ITouch::SCancelEvent{.touchID = CONTACT});
            // This adapter only handles gesture cancellation. Keep ordinary client contacts
            // in the fixture's teardown list until their explicit up.
            if (CONSUMED)
                std::erase(g_contacts[DEVICE], CONTACT);
        } else
            return luaL_error(L, "Unknown synthetic touch action");
        return 0;
    }

    static int luaInvalidEnd(lua_State* L) {
        if (!g_swiping || !g_pUnifiedWorkspaceSwipe->isGestureInProgress() || !*CConfigValue<Config::INTEGER>("gestures:workspace_swipe_use_r") ||
            State::Workspace::resolver()->getWorkspaceTargetFromString("r+1").valid())
            return luaL_error(L, "Invalid-end probe requires a live preview and an invalid real r+1 target");
        g_pUnifiedWorkspaceSwipe->end();
        return 0;
    }

    static void numberField(lua_State* L, const char* name, double value) {
        lua_pushnumber(L, value);
        lua_setfield(L, -2, name);
    }

    static void boolField(lua_State* L, const char* name, bool value) {
        lua_pushboolean(L, value);
        lua_setfield(L, -2, name);
    }

    static int luaTouchSnapshot(lua_State* L) {
        const auto DEVICE = luaL_checkinteger(L, 1);
        const auto ID     = luaL_checkinteger(L, 2);
        if (DEVICE < 0 || DEVICE >= sc<lua_Integer>(g_touches.size()) || !g_touches[DEVICE])
            return luaL_error(L, "No such synthetic touch device");
        const auto& DATA = g_pInputManager->m_touchData;
        lua_newtable(L);
        boolField(L, "consumed", std::ranges::any_of(DATA.consumedTouches, [&](const auto& contact) { return contact.device == g_touches[DEVICE] && contact.id == ID; }));
        boolField(L, "owner", DATA.workspaceSwipe && DATA.workspaceSwipe->device == g_touches[DEVICE] && DATA.workspaceSwipe->id == ID);
        boolField(L, "client_focus", !g_pSeatManager->m_state.touchFocus.expired());
        numberField(L, "focus_events", g_touchFocusEvents);
        return 1;
    }

    static int luaSnapshot(lua_State* L) {
        const auto SELECTOR = luaL_checkstring(L, 1);
        // With disabled animations, an explicit tick settles pending focus/end transitions
        // synchronously. Never rely on the time between socket calls for exact assertions.
        Animation::mgr()->tick();
        const auto WORKSPACE = State::Workspace::state()->find(State::Workspace::resolver()->getWorkspaceTargetFromString(SELECTOR));
        lua_newtable(L);
        boolField(L, "exists", !!WORKSPACE);
        boolField(L, "swiping", g_pUnifiedWorkspaceSwipe->isGestureInProgress());
        lua_pushinteger(L, g_pUnifiedWorkspaceSwipe->sessionID());
        lua_setfield(L, -2, "session_id");
        if (!WORKSPACE)
            return 1;

        numberField(L, "alpha", WORKSPACE->m_alpha->value());
        numberField(L, "alpha_goal", WORKSPACE->m_alpha->goal());
        numberField(L, "offset_x", WORKSPACE->m_renderOffset->value().x);
        numberField(L, "offset_y", WORKSPACE->m_renderOffset->value().y);
        numberField(L, "offset_goal_x", WORKSPACE->m_renderOffset->goal().x);
        numberField(L, "offset_goal_y", WORKSPACE->m_renderOffset->goal().y);
        boolField(L, "forced", WORKSPACE->m_forceRendering);
        boolField(L, "mapped", WORKSPACE->visible());
        const auto MONITOR = WORKSPACE->m_monitor.lock();
        boolField(L, "active", MONITOR && MONITOR->m_activeWorkspace == WORKSPACE);
        lua_pushstring(L, MONITOR ? MONITOR->m_name.c_str() : "");
        lua_setfield(L, -2, "monitor");
        lua_pushstring(L, WORKSPACE->addressableName().c_str());
        lua_setfield(L, -2, "address");
        return 1;
    }

    void registerFunctions() {
        const std::pair<const char*, PLUGIN_LUA_FN> FUNCTIONS[] = {
            {"workspace_swipe_fixture", luaFixture},
            {"workspace_swipe_reset", luaReset},
            {"swipe_begin", luaBegin},
            {"swipe_update", luaUpdate},
            {"swipe_end", luaEnd},
            {"workspace_swipe_cancel", luaCancel},
            {"workspace_swipe_invalid_end", luaInvalidEnd},
            {"workspace_snapshot", luaSnapshot},
            {"workspace_swipe_peer", luaPeer},
            {"workspace_swipe_remove_origin_output", luaRemoveOriginOutput},
            {"workspace_swipe_adapter_probe", luaProbeAdapter},
            {"workspace_swipe_reentrant_end_probe", luaProbeReentrantEnd},
            {"workspace_swipe_touch_setup", luaTouchSetup},
            {"workspace_swipe_touch", luaTouch},
            {"workspace_swipe_touch_snapshot", luaTouchSnapshot},
        };
        for (const auto& [name, fn] : FUNCTIONS) {
            if (!HyprlandAPI::addLuaFunction(PHANDLE, "test", name, fn))
                LOG(Log::ERR, "hyprtester plugin: failed to register hl.plugin.test.{}", name);
        }
    }
}
