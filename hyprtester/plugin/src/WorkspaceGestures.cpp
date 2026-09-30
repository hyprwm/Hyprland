#include "WorkspaceGestures.hpp"
#include "globals.hpp"

#include <src/animation/AnimationManager.hpp>
#include <src/config/ConfigValue.hpp>
#include <src/desktop/state/FocusState.hpp>
#include <src/managers/input/UnifiedWorkspaceSwipeGesture.hpp>
#include <src/managers/input/trackpad/TrackpadGestures.hpp>
#include <src/output/Monitor.hpp>
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
    // Holds keep empty fixture workspaces alive between staged Lua calls.
    static std::array<PHLWORKSPACE, 3> g_workspaces;
    static PHLWORKSPACE                g_previous;
    static PHLMONITORREF               g_monitor;
    static bool                        g_swiping = false;
    static uint32_t                    g_fingers = 0;

    void                               reset() {
        if (g_previous) {
            g_pUnifiedWorkspaceSwipe->cancel();
            if (g_swiping)
                g_pTrackpadGestures->gestureEnd(IPointer::SSwipeEndEvent{.cancelled = true});

            if (const auto MONITOR = g_monitor.lock())
                MONITOR->changeWorkspace(g_previous);

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

    static int luaSnapshot(lua_State* L) {
        const auto SELECTOR = luaL_checkstring(L, 1);
        // With disabled animations, an explicit tick settles pending focus/end transitions
        // synchronously. Never rely on the time between socket calls for exact assertions.
        Animation::mgr()->tick();
        const auto WORKSPACE = State::Workspace::state()->find(State::Workspace::resolver()->getWorkspaceTargetFromString(SELECTOR));
        lua_newtable(L);
        boolField(L, "exists", !!WORKSPACE);
        boolField(L, "swiping", g_pUnifiedWorkspaceSwipe->isGestureInProgress());
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
        };
        for (const auto& [name, fn] : FUNCTIONS) {
            if (!HyprlandAPI::addLuaFunction(PHANDLE, "test", name, fn))
                LOG(Log::ERR, "hyprtester plugin: failed to register hl.plugin.test.{}", name);
        }
    }
}
