#include "SpecialWorkspaceGestures.hpp"
#include "globals.hpp"

#include <src/animation/AnimationManager.hpp>
#include <src/config/ConfigValue.hpp>
#include <src/config/shared/animation/AnimationTree.hpp>
#include <src/desktop/state/FocusState.hpp>
#include <src/event/EventBus.hpp>
#include <src/managers/input/trackpad/gestures/ITrackpadGesture.hpp>
#include <src/output/Monitor.hpp>
#include <src/state/MonitorState.hpp>
#include <src/state/workspace/State.hpp>
#include <src/workspace/HLWorkspace.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

// Inspect lifetime bookkeeping without adding a production test API. Drive only real gesture methods.
#define private public
#include <src/managers/input/trackpad/gestures/SpecialWorkspaceGesture.hpp>
#undef private

#include <array>
#include <cmath>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace SpecialWorkspaceGestures {
    static constexpr auto               NAME = "hyprtester-special-swipe";

    static UP<CSpecialWorkspaceGesture> g_gesture;
    static PHLWORKSPACE                 g_workspace;
    static PHLWORKSPACE                 g_replacement;
    static PHLWORKSPACEREF              g_workspaceRef;
    static PHLMONITORREF                g_monitor;

    static void                         releaseWorkspace(PHLWORKSPACE workspace) {
        if (!workspace)
            return;
        const auto MONITOR = workspace->m_monitor.lock();
        if (MONITOR && MONITOR->m_enabled && MONITOR->m_activeSpecialWorkspace == workspace) {
            MONITOR->setSpecialWorkspace(nullptr, true);
            MONITOR->m_specialFade->warp();
            MONITOR->m_specialDim->warp();
            MONITOR->m_specialBlur->warp();
        }
        workspace->m_forceRendering = false;
        workspace->m_alpha->setValueAndWarp(0.F);
        workspace->m_renderOffset->setValueAndWarp({});
    }

    void reset() {
        g_gesture.reset();
        releaseWorkspace(g_workspaceRef.lock());
        releaseWorkspace(g_replacement);
        g_workspace.reset();
        g_replacement.reset();
        g_workspaceRef.reset();
        g_monitor.reset();
    }

    static std::string prepareFixture(bool opened, bool forced) {
        reset();
        const auto MONITOR = Desktop::focusState()->monitor();
        if (!MONITOR || !MONITOR->m_enabled || !MONITOR->m_activeWorkspace || MONITOR->m_activeSpecialWorkspace)
            return "Special swipe fixture requires an enabled monitor without an active special workspace";

        bool                          ready = false;
        Hyprutils::Utils::CScopeGuard cleanup([&] {
            if (!ready)
                reset();
        });
        g_monitor      = MONITOR;
        g_workspace    = State::Workspace::state()->createSpecial(NAME, MONITOR);
        g_workspaceRef = g_workspace;
        if (!g_workspace || g_workspace->m_monitor != MONITOR)
            return "Could not create the special swipe fixture";

        MONITOR->m_specialFade->warp();
        MONITOR->m_specialDim->warp();
        MONITOR->m_specialBlur->warp();
        if (opened)
            MONITOR->setSpecialWorkspace(g_workspace);
        g_workspace->m_forceRendering = forced;
        g_gesture                     = makeUnique<CSpecialWorkspaceGesture>(NAME);
        ready                         = true;
        return {};
    }

    static void begin() {
        g_gesture->begin({.direction = TRACKPAD_GESTURE_DIR_HORIZONTAL});
    }

    static void update(double delta) {
        const IPointer::SSwipeUpdateEvent EVENT{.fingers = 3, .delta = {delta, 0.0}};
        g_gesture->update({.swipe = &EVENT, .direction = TRACKPAD_GESTURE_DIR_HORIZONTAL});
    }

    static void end(bool cancelled) {
        const IPointer::SSwipeEndEvent EVENT{.cancelled = cancelled};
        g_gesture->end({.swipe = &EVENT, .direction = TRACKPAD_GESTURE_DIR_HORIZONTAL});
    }

    static bool sessionReleased() {
        return g_gesture && !g_gesture->m_sessionActive && !g_gesture->m_forceRenderingAcquired && !g_gesture->m_specialWorkspace && !g_gesture->m_monitor &&
            !g_gesture->m_hasPreview;
    }

    struct SSnapshot {
        std::array<double, 12>                             values = {};
        PHLWORKSPACEREF                                    active;
        PHLMONITORREF                                      owner;
        bool                                               visible         = false;
        bool                                               forced          = false;
        bool                                               alphaAnimating  = false;
        bool                                               offsetAnimating = false;
        WP<Hyprutils::Animation::SAnimationPropertyConfig> alphaConfig;
        WP<Hyprutils::Animation::SAnimationPropertyConfig> offsetConfig;

        bool                                               operator==(const SSnapshot&) const = default;
    };

    static SSnapshot snapshot(PHLWORKSPACE workspace, PHLMONITOR monitor) {
        SSnapshot result;
        if (workspace) {
            result.values[0]       = workspace->m_alpha->value();
            result.values[1]       = workspace->m_alpha->goal();
            result.values[2]       = workspace->m_renderOffset->value().x;
            result.values[3]       = workspace->m_renderOffset->value().y;
            result.values[4]       = workspace->m_renderOffset->goal().x;
            result.values[5]       = workspace->m_renderOffset->goal().y;
            result.owner           = workspace->m_monitor;
            result.visible         = workspace->visible();
            result.forced          = workspace->m_forceRendering;
            result.alphaAnimating  = workspace->m_alpha->isBeingAnimated();
            result.offsetAnimating = workspace->m_renderOffset->isBeingAnimated();
            result.alphaConfig     = workspace->m_alpha->getConfig();
            result.offsetConfig    = workspace->m_renderOffset->getConfig();
        }
        if (monitor) {
            result.values[6]  = monitor->m_specialFade->value();
            result.values[7]  = monitor->m_specialFade->goal();
            result.values[8]  = monitor->m_specialDim->value();
            result.values[9]  = monitor->m_specialDim->goal();
            result.values[10] = monitor->m_specialBlur->value();
            result.values[11] = monitor->m_specialBlur->goal();
            result.active     = monitor->m_activeSpecialWorkspace;
        }
        return result;
    }

    static std::string probeMove(const PHLMONITOR& monitor, const char* peerName, std::string_view overrideMode) {
        const auto PEER = State::monitorState()->query().name(peerName).run();
        if (!PEER || !PEER->m_enabled || PEER == monitor || PEER->m_activeSpecialWorkspace)
            return "Move probe requires a different enabled output without a special workspace";
        if (overrideMode != "none" && overrideMode != "alpha" && overrideMode != "offset" && overrideMode != "both" && overrideMode != "animated" &&
            overrideMode != "animated-same" && overrideMode != "unupdated")
            return "Unknown move override mode";

        // Replacing a destination special emits its activeChanged before the incoming workspace's
        // monitorChanged. Install external state there, before gesture invalidation can run.
        g_replacement = State::Workspace::state()->createSpecial("hyprtester-special-replacement", PEER);
        if (!g_replacement)
            return "Could not create the destination special workspace";
        PEER->setSpecialWorkspace(g_replacement, true);
        bool       observed = false;
        SSnapshot  before, sourceBefore, destinationBefore;
        const auto LISTENER = g_replacement->m_events.activeChanged.listen([&] {
            if (PEER->m_activeSpecialWorkspace != g_workspace)
                return;
            observed = true;
            if (overrideMode == "alpha" || overrideMode == "both")
                g_workspace->m_alpha->setValueAndWarp(0.625F);
            if (overrideMode == "offset" || overrideMode == "both")
                g_workspace->m_renderOffset->setValueAndWarp({17, -9});
            if (overrideMode == "animated" || overrideMode == "animated-same") {
                const auto CONFIG = Config::animationTree()->getAnimationPropertyConfig("specialWorkspaceOut");
                const auto ALPHA  = g_workspace->m_alpha->value();
                const auto OFFSET = g_workspace->m_renderOffset->value();
                g_workspace->m_alpha->setConfig(CONFIG);
                g_workspace->m_renderOffset->setConfig(CONFIG);
                *g_workspace->m_alpha        = 0.875F;
                *g_workspace->m_renderOffset = Vector2D{-13, 19};
                if (overrideMode == "animated-same") {
                    // Same value and goal as our preview, but a new animation owns the property.
                    *g_workspace->m_alpha        = ALPHA;
                    *g_workspace->m_renderOffset = OFFSET;
                }
            }
            before            = snapshot(g_workspace, nullptr);
            sourceBefore      = snapshot(nullptr, monitor);
            destinationBefore = snapshot(nullptr, PEER);
        });
        PEER->setSpecialWorkspace(g_workspace);
        if (!observed || g_workspace->m_monitor != PEER || PEER->m_activeSpecialWorkspace != g_workspace || !g_workspace->visible())
            return "Special workspace did not visibly replace the destination special";
        if (snapshot(nullptr, monitor) != sourceBefore || snapshot(nullptr, PEER) != destinationBefore)
            return "Move cleanup changed source or destination monitor fade/dim/blur";

        const auto AFTER = snapshot(g_workspace, nullptr);
        if (AFTER.alphaConfig != before.alphaConfig || AFTER.offsetConfig != before.offsetConfig)
            return "Move cleanup replaced external animation configuration";
        if (overrideMode == "alpha" || overrideMode == "both" || overrideMode == "animated" || overrideMode == "animated-same" || overrideMode == "unupdated") {
            if (AFTER.values[0] != before.values[0] || AFTER.values[1] != before.values[1] || AFTER.alphaAnimating != before.alphaAnimating)
                return "Move cleanup overwrote external alpha state";
        } else if (g_workspace->m_alpha->goal() != 1.F)
            return "Moved active special was left at the gesture's partial alpha instead of completing IN";
        if (overrideMode == "offset" || overrideMode == "both" || overrideMode == "animated" || overrideMode == "animated-same" || overrideMode == "unupdated") {
            for (size_t i = 2; i < 6; ++i) {
                if (AFTER.values[i] != before.values[i])
                    return "Move cleanup overwrote external offset state";
            }
            if (AFTER.offsetAnimating != before.offsetAnimating)
                return "Move cleanup stopped an external offset animation";
        } else if (g_workspace->m_renderOffset->goal() != Vector2D{})
            return "Moved active special was left at the gesture's partial offset instead of completing IN";
        if ((overrideMode == "animated" || overrideMode == "animated-same") && (!before.alphaAnimating || !before.offsetAnimating))
            return "External animation override was not established";
        return {};
    }

    static std::string probeInvalidation(std::string_view action, const char* peerName, std::string_view overrideMode) {
        const auto MONITOR = g_monitor.lock();
        if (!g_gesture || !g_gesture->m_sessionActive || !g_workspace || !MONITOR)
            return "Invalidation probe requires a live special swipe";
        const bool FORCED = !g_gesture->m_forceRenderingAcquired;

        if (action == "replace") {
            g_replacement = State::Workspace::state()->createSpecial("hyprtester-special-replacement", MONITOR);
            if (!g_replacement)
                return "Could not create replacement special workspace";
            MONITOR->setSpecialWorkspace(g_replacement);
            // Distinct external values expose late cleanup that writes cached goals or neutral values.
            g_replacement->m_alpha->setValueAndWarp(0.625F);
            g_replacement->m_renderOffset->setValueAndWarp({17, -9});
            g_replacement->m_forceRendering = true;
            MONITOR->m_specialFade->setValueAndWarp(0.375F);
            MONITOR->m_specialDim->setValueAndWarp(0.25F);
            MONITOR->m_specialBlur->setValueAndWarp(0.125F);
        } else if (action == "move") {
            if (const auto ERROR = probeMove(MONITOR, peerName, overrideMode); !ERROR.empty())
                return ERROR;
        } else if (action == "remove") {
            if (MONITOR->m_name != "HYPRTEST-SPECIAL-A")
                return "Only the dedicated special swipe output may be removed";
            auto before   = snapshot(g_workspace, MONITOR);
            before.forced = FORCED;
            bool observed = false, untouched = false, released = false;
            // Registered after the gesture listener: inspect cleanup before monitor migration runs.
            const auto LISTENER = MONITOR->m_events.disconnect.listen([&] {
                observed  = true;
                untouched = snapshot(g_workspace, MONITOR) == before;
                released  = sessionReleased();
            });
            if (HyprlandAPI::invokeHyprctlCommand("output", "remove " + MONITOR->m_name) != "ok")
                return "Could not remove the special swipe output";
            if (!observed || !released || !untouched)
                return "Disconnect cleanup did not release the session without mutating mid-teardown state";
        } else
            return "Unknown special swipe invalidation action";

        // This assertion precedes every late event: an end/update cannot hide a missing signal listener.
        if (!sessionReleased() || g_workspace->m_forceRendering != FORCED)
            return "Invalidation did not immediately release the session and its owned rendering hold";
        const auto OWNER       = g_workspace->m_monitor.lock();
        const auto WORKSPACE   = snapshot(g_workspace, OWNER);
        const auto REPLACEMENT = snapshot(g_replacement, MONITOR);
        update(150);
        end(false);
        end(true);
        g_gesture.reset();
        if (snapshot(g_workspace, OWNER) != WORKSPACE || snapshot(g_replacement, MONITOR) != REPLACEMENT)
            return "Late update/end/destructor changed external special workspace state";
        return {};
    }

    static std::string probeReentrantEnd() {
        const auto MONITOR = g_monitor.lock();
        if (!g_gesture || !g_gesture->m_sessionActive || g_gesture->m_animatingOut || !MONITOR)
            return "Reentrant probe requires an opening special swipe below the commit threshold";
        bool       called = false;
        SSnapshot  replacement;
        const auto LISTENER = Event::bus()->m_events.workspace.specialActive.listen([&](PHLWORKSPACE workspace, PHLMONITOR monitor) {
            if (called || workspace || monitor != MONITOR)
                return;
            called = true;
            begin();
            update(75);
            replacement = snapshot(g_workspace, MONITOR);
        });
        end(false);
        if (!called || !g_gesture->m_sessionActive || !g_workspace->m_forceRendering || snapshot(g_workspace, MONITOR) != replacement)
            return "Outer end overwrote or released the reentrant special swipe";
        return {};
    }

    static int luaFixture(lua_State* L) {
        luaL_checktype(L, 1, LUA_TBOOLEAN);
        luaL_checktype(L, 2, LUA_TBOOLEAN);
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
        if (!g_gesture)
            return luaL_error(L, "Prepare a special swipe fixture before beginning");
        begin();
        return 0;
    }

    static int luaUpdate(lua_State* L) {
        const auto DELTA = luaL_checknumber(L, 1);
        if (!g_gesture || !std::isfinite(DELTA))
            return luaL_error(L, "Expected a gesture object and a finite delta");
        update(DELTA);
        return 0;
    }

    static int luaEnd(lua_State* L) {
        luaL_checktype(L, 1, LUA_TBOOLEAN);
        if (!g_gesture)
            return luaL_error(L, "No special swipe gesture object");
        end(lua_toboolean(L, 1));
        return 0;
    }

    static int luaDestroy(lua_State* L) {
        g_gesture.reset();
        return 0;
    }

    static int luaDropWorkspace(lua_State* L) {
        g_workspace.reset();
        return 0;
    }

    static int luaSettle(lua_State* L) {
        if (*CConfigValue<Config::INTEGER>("animations:enabled"))
            return luaL_error(L, "Synchronous settling requires disabled animations");
        Animation::mgr()->tick();
        return 0;
    }

    static int luaInvalidate(lua_State* L) {
        const auto ACTION   = luaL_checkstring(L, 1);
        const auto PEER     = luaL_optstring(L, 2, "");
        const auto OVERRIDE = luaL_optstring(L, 3, "none");
        bool       failed   = false;
        {
            const auto ERROR = probeInvalidation(ACTION, PEER, OVERRIDE);
            failed           = !ERROR.empty();
            if (failed)
                lua_pushstring(L, ERROR.c_str());
        }
        return failed ? lua_error(L) : 0;
    }

    static int luaReentrantEnd(lua_State* L) {
        bool failed = false;
        {
            const auto ERROR = probeReentrantEnd();
            failed           = !ERROR.empty();
            if (failed)
                lua_pushstring(L, ERROR.c_str());
        }
        return failed ? lua_error(L) : 0;
    }

    static void boolField(lua_State* L, const char* name, bool value) {
        lua_pushboolean(L, value);
        lua_setfield(L, -2, name);
    }

    static int luaSnapshot(lua_State* L) {
        const auto WORKSPACE = g_workspaceRef.lock();
        const auto MONITOR   = WORKSPACE ? WORKSPACE->m_monitor.lock() : g_monitor.lock();
        const auto SNAPSHOT  = snapshot(WORKSPACE, MONITOR);
        lua_newtable(L);
        boolField(L, "exists", !!WORKSPACE);
        boolField(L, "gesture_alive", !!g_gesture);
        boolField(L, "session", g_gesture && g_gesture->m_sessionActive);
        boolField(L, "released", sessionReleased());
        boolField(L, "acquired", g_gesture && g_gesture->m_forceRenderingAcquired);
        boolField(L, "forced", SNAPSHOT.forced);
        boolField(L, "visible", SNAPSHOT.visible);
        boolField(L, "active", WORKSPACE && SNAPSHOT.active == WORKSPACE);
        const char* NAMES[] = {
            "alpha", "alpha_goal", "offset_x", "offset_y", "offset_goal_x", "offset_goal_y", "fade", "fade_goal", "dim", "dim_goal", "blur", "blur_goal",
        };
        for (size_t i = 0; i < SNAPSHOT.values.size(); ++i) {
            lua_pushnumber(L, SNAPSHOT.values[i]);
            lua_setfield(L, -2, NAMES[i]);
        }
        return 1;
    }

    void registerFunctions() {
        const std::pair<const char*, PLUGIN_LUA_FN> FUNCTIONS[] = {
            {"special_swipe_fixture", luaFixture},
            {"special_swipe_reset", luaReset},
            {"special_swipe_begin", luaBegin},
            {"special_swipe_update", luaUpdate},
            {"special_swipe_end", luaEnd},
            {"special_swipe_destroy", luaDestroy},
            {"special_swipe_drop_workspace", luaDropWorkspace},
            {"special_swipe_settle", luaSettle},
            {"special_swipe_invalidate", luaInvalidate},
            {"special_swipe_reentrant_end", luaReentrantEnd},
            {"special_swipe_snapshot", luaSnapshot},
        };
        for (const auto& [name, fn] : FUNCTIONS) {
            if (!HyprlandAPI::addLuaFunction(PHANDLE, "test", name, fn))
                LOG(Log::ERR, "hyprtester plugin: failed to register hl.plugin.test.{}", name);
        }
    }
}
