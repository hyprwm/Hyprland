#include "LuaBindingsInternal.hpp"

#include "../objects/LuaLayerSurface.hpp"
#include "../objects/LuaMonitor.hpp"
#include "../objects/LuaWindow.hpp"
#include "../objects/LuaWorkspace.hpp"

#include "../../../desktop/history/WindowHistoryTracker.hpp"
#include "../../../desktop/history/WorkspaceHistoryTracker.hpp"
#include "../../../desktop/rule/windowRule/WindowRuleEffectContainer.hpp"
#include "../../../desktop/view/LayerSurface.hpp"
#include "../../../desktop/view/Window.hpp"
#include "../../../managers/input/InputManager.hpp"

using namespace Config;
using namespace Config::Lua;
using namespace Config::Lua::Bindings;

namespace {
    struct SWindowQuery {
        std::optional<PHLMONITOR>   monitor;
        std::optional<PHLWORKSPACE> workspace;
        std::optional<bool>         floating;
        std::optional<bool>         mapped;
        std::optional<std::string>  className;
        std::optional<std::string>  title;
        std::optional<std::string>  tag;
    };

    struct SLayerQuery {
        std::optional<PHLMONITOR>  monitor;
        std::optional<std::string> namespace_;
    };
}

static bool windowMatchesQuery(const PHLWINDOW& w, const SWindowQuery& query) {
    if (query.monitor) {
        const auto mon = w->m_monitor.lock();
        if (mon != *query.monitor)
            return false;
    }

    if (query.workspace && w->m_workspace != *query.workspace)
        return false;

    if (query.floating && w->m_isFloating != *query.floating)
        return false;

    if (query.mapped && w->m_isMapped != *query.mapped)
        return false;

    if (query.className && w->m_class != *query.className)
        return false;

    if (query.title && w->m_title != *query.title)
        return false;

    if (query.tag) {
        if (!w->m_ruleApplicator || !w->m_ruleApplicator->m_tagKeeper.isTagged(*query.tag, true))
            return false;
    }

    return true;
}

static void pushWindowsMatchingQuery(lua_State* L, const SWindowQuery& query) {
    lua_newtable(L);

    int i = 1;
    for (const auto& w : g_pCompositor->m_windows) {
        if (!windowMatchesQuery(w, query))
            continue;

        Objects::CLuaWindow::push(L, w);
        lua_rawseti(L, -2, i++);
    }
}

static void parseWindowQueryFromTable(lua_State* L, int idx, const char* fnName, SWindowQuery& query) {
    query.monitor   = Internal::tableOptMonitor(L, idx, "monitor", fnName);
    query.workspace = Internal::tableOptWorkspace(L, idx, "workspace", fnName);

    if (const auto floating = Internal::tableOptBool(L, idx, "floating"); floating.has_value())
        query.floating = floating;

    if (const auto mapped = Internal::tableOptBool(L, idx, "mapped"); mapped.has_value())
        query.mapped = mapped;

    query.className = Internal::tableOptStr(L, idx, "class");
    query.title     = Internal::tableOptStr(L, idx, "title");
    query.tag       = Internal::tableOptStr(L, idx, "tag");
}

static void parseLayerQueryFromTable(lua_State* L, int idx, const char* fnName, SLayerQuery& query) {
    query.monitor    = Internal::tableOptMonitor(L, idx, "monitor", fnName);
    query.namespace_ = Internal::tableOptStr(L, idx, "namespace");
}

static PHLMONITOR monitorFromOptionalArg(lua_State* L, int idx, const char* fnName) {
    if (lua_gettop(L) < idx || lua_isnil(L, idx))
        return Desktop::focusState()->monitor();

    return Internal::monitorFromLuaSelectorOrObject(L, idx, fnName);
}

static int hlGetWindows(lua_State* L) {
    SWindowQuery query;
    query.mapped = true;

    if (lua_gettop(L) >= 1 && !lua_isnil(L, 1)) {
        if (!lua_istable(L, 1))
            return Internal::configError(L, "hl.get_windows: expected no args or a table of filters");

        parseWindowQueryFromTable(L, 1, "hl.get_windows", query);
    }

    pushWindowsMatchingQuery(L, query);
    return 1;
}

static int hlGetActiveWindow(lua_State* L) {
    auto PWINDOW = Desktop::focusState()->window();
    if (!PWINDOW) {
        lua_pushnil(L);
        return 1;
    }

    Config::Lua::Objects::CLuaWindow::push(L, PWINDOW);
    return 1;
}

static int hlGetWindow(lua_State* L) {
    const auto PWINDOW = Internal::windowFromLuaSelectorOrObject(L, 1, "hl.get_window");
    if (!PWINDOW) {
        lua_pushnil(L);
        return 1;
    }

    Objects::CLuaWindow::push(L, PWINDOW);
    return 1;
}

static int hlGetUrgentWindow(lua_State* L) {
    const auto PWINDOW = g_pCompositor->getUrgentWindow();
    if (!PWINDOW) {
        lua_pushnil(L);
        return 1;
    }

    Objects::CLuaWindow::push(L, PWINDOW);
    return 1;
}

static int hlGetWorkspaces(lua_State* L) {
    lua_newtable(L);
    int i = 1;
    for (const auto& wsRef : g_pCompositor->getWorkspaces()) {
        const auto ws = wsRef.lock();
        if (!ws || ws->inert())
            continue;
        Objects::CLuaWorkspace::push(L, ws);
        lua_rawseti(L, -2, i++);
    }
    return 1;
}

static int hlGetWorkspace(lua_State* L) {
    const auto PWORKSPACE = Internal::workspaceFromLuaSelectorOrObject(L, 1, "hl.get_workspace");
    if (!PWORKSPACE) {
        lua_pushnil(L);
        return 1;
    }

    Objects::CLuaWorkspace::push(L, PWORKSPACE);
    return 1;
}

static int hlGetActiveWorkspace(lua_State* L) {
    const auto PMONITOR = monitorFromOptionalArg(L, 1, "hl.get_active_workspace");
    if (!PMONITOR || !PMONITOR->m_activeWorkspace) {
        lua_pushnil(L);
        return 1;
    }

    Objects::CLuaWorkspace::push(L, PMONITOR->m_activeWorkspace);
    return 1;
}

static int hlGetActiveSpecialWorkspace(lua_State* L) {
    const auto PMONITOR = monitorFromOptionalArg(L, 1, "hl.get_active_special_workspace");
    if (!PMONITOR || !PMONITOR->m_activeSpecialWorkspace) {
        lua_pushnil(L);
        return 1;
    }

    Objects::CLuaWorkspace::push(L, PMONITOR->m_activeSpecialWorkspace);
    return 1;
}

static int hlGetMonitors(lua_State* L) {
    lua_newtable(L);
    int i = 1;
    for (const auto& mon : g_pCompositor->m_monitors) {
        Objects::CLuaMonitor::push(L, mon);
        lua_rawseti(L, -2, i++);
    }
    return 1;
}

static int hlGetMonitor(lua_State* L) {
    const auto PMONITOR = Internal::monitorFromLuaSelectorOrObject(L, 1, "hl.get_monitor");
    if (!PMONITOR) {
        lua_pushnil(L);
        return 1;
    }

    Objects::CLuaMonitor::push(L, PMONITOR);
    return 1;
}

static int hlGetActiveMonitor(lua_State* L) {
    const auto PMONITOR = Desktop::focusState()->monitor();
    if (!PMONITOR) {
        lua_pushnil(L);
        return 1;
    }

    Objects::CLuaMonitor::push(L, PMONITOR);
    return 1;
}

static int hlGetMonitorAt(lua_State* L) {
    double x = 0;
    double y = 0;

    if (lua_istable(L, 1)) {
        const auto tx = Internal::tableOptNum(L, 1, "x");
        const auto ty = Internal::tableOptNum(L, 1, "y");
        if (!tx || !ty)
            return Internal::configError(L, "hl.get_monitor_at: expected a table { x, y }");

        x = *tx;
        y = *ty;
    } else {
        x = luaL_checknumber(L, 1);
        y = luaL_checknumber(L, 2);
    }

    const auto PMONITOR = g_pCompositor->getMonitorFromVector(Vector2D{x, y});
    if (!PMONITOR) {
        lua_pushnil(L);
        return 1;
    }

    Objects::CLuaMonitor::push(L, PMONITOR);
    return 1;
}

static int hlGetMonitorAtCursor(lua_State* L) {
    const auto PMONITOR = g_pCompositor->getMonitorFromCursor();
    if (!PMONITOR) {
        lua_pushnil(L);
        return 1;
    }

    Objects::CLuaMonitor::push(L, PMONITOR);
    return 1;
}

static int hlGetCursorPos(lua_State* L) {
    if (!g_pInputManager) {
        lua_pushnil(L);
        return 1;
    }

    const auto pos = g_pInputManager->getMouseCoordsInternal();

    lua_newtable(L);
    lua_pushnumber(L, pos.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, pos.y);
    lua_setfield(L, -2, "y");
    return 1;
}

static int hlGetLastWindow(lua_State* L) {
    const auto  current     = Desktop::focusState()->window();
    const auto& fullHistory = Desktop::History::windowTracker()->fullHistory();

    for (auto it = fullHistory.rbegin(); it != fullHistory.rend(); ++it) {
        const auto candidate = it->lock();
        if (!candidate || !candidate->m_isMapped)
            continue;

        if (current && candidate == current)
            continue;

        Objects::CLuaWindow::push(L, candidate);
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

static int hlGetLastWorkspace(lua_State* L) {
    const bool hadMonitorArg = lua_gettop(L) >= 1 && !lua_isnil(L, 1);
    const auto PMONITOR      = hadMonitorArg ? Internal::monitorFromLuaSelectorOrObject(L, 1, "hl.get_last_workspace") : Desktop::focusState()->monitor();
    if (!PMONITOR || !PMONITOR->m_activeWorkspace) {
        lua_pushnil(L);
        return 1;
    }

    const auto current = PMONITOR->m_activeWorkspace;

    auto previous = hadMonitorArg ? Desktop::History::workspaceTracker()->previousWorkspace(current, PMONITOR) : Desktop::History::workspaceTracker()->previousWorkspace(current);

    auto ws = previous.workspace.lock();
    if ((!ws || ws->inert()) && previous.id != WORKSPACE_INVALID)
        ws = g_pCompositor->getWorkspaceByID(previous.id);

    if (!ws || ws->inert()) {
        lua_pushnil(L);
        return 1;
    }

    Objects::CLuaWorkspace::push(L, ws);
    return 1;
}

static int hlGetLayers(lua_State* L) {
    SLayerQuery query;

    if (lua_gettop(L) >= 1 && !lua_isnil(L, 1)) {
        if (!lua_istable(L, 1))
            return Internal::configError(L, "hl.get_layers: expected no args or a table of filters");

        parseLayerQueryFromTable(L, 1, "hl.get_layers", query);
    }

    lua_newtable(L);
    int i = 1;
    for (const auto& mon : g_pCompositor->m_monitors) {
        if (query.monitor && mon != *query.monitor)
            continue;

        for (const auto& level : mon->m_layerSurfaceLayers) {
            for (const auto& lsRef : level) {
                const auto ls = lsRef.lock();
                if (!ls)
                    continue;

                if (query.namespace_ && ls->m_namespace != *query.namespace_)
                    continue;

                Objects::CLuaLayerSurface::push(L, ls);
                lua_rawseti(L, -2, i++);
            }
        }
    }
    return 1;
}

static int hlGetWorkspaceWindows(lua_State* L) {
    const auto   ws = Internal::workspaceFromLuaSelectorOrObject(L, 1, "hl.get_workspace_windows");

    SWindowQuery query;
    query.workspace = ws;
    query.mapped    = true;

    pushWindowsMatchingQuery(L, query);
    return 1;
}

static int hlGetCurrentSubmap(lua_State* L) {
    lua_pushstring(L, Config::Actions::state()->m_currentSubmap.c_str());
    return 1;
}

void Internal::registerQueryBindings(lua_State* L) {
    Internal::setFn(L, "get_windows", hlGetWindows);
    Internal::setFn(L, "get_window", hlGetWindow);
    Internal::setFn(L, "get_active_window", hlGetActiveWindow);
    Internal::setFn(L, "get_urgent_window", hlGetUrgentWindow);
    Internal::setFn(L, "get_workspaces", hlGetWorkspaces);
    Internal::setFn(L, "get_workspace", hlGetWorkspace);
    Internal::setFn(L, "get_active_workspace", hlGetActiveWorkspace);
    Internal::setFn(L, "get_active_special_workspace", hlGetActiveSpecialWorkspace);
    Internal::setFn(L, "get_monitors", hlGetMonitors);
    Internal::setFn(L, "get_monitor", hlGetMonitor);
    Internal::setFn(L, "get_active_monitor", hlGetActiveMonitor);
    Internal::setFn(L, "get_monitor_at", hlGetMonitorAt);
    Internal::setFn(L, "get_monitor_at_cursor", hlGetMonitorAtCursor);
    Internal::setFn(L, "get_layers", hlGetLayers);
    Internal::setFn(L, "get_workspace_windows", hlGetWorkspaceWindows);
    Internal::setFn(L, "get_cursor_pos", hlGetCursorPos);
    Internal::setFn(L, "get_last_window", hlGetLastWindow);
    Internal::setFn(L, "get_last_workspace", hlGetLastWorkspace);
    Internal::setFn(L, "get_current_submap", hlGetCurrentSubmap);
}
