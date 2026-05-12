#include "LuaWorkspace.hpp"
#include "LuaMonitor.hpp"
#include "LuaWindow.hpp"
#include "LuaGroup.hpp"
#include "LuaObjectHelpers.hpp"

#include "../../../desktop/Workspace.hpp"
#include "../../../desktop/view/Group.hpp"
#include "../../../helpers/Monitor.hpp"
#include "../../../layout/space/Space.hpp"
#include "../../../layout/algorithm/Algorithm.hpp"
#include "../../../layout/algorithm/TiledAlgorithm.hpp"
#include "../../../layout/supplementary/WorkspaceAlgoMatcher.hpp"
#include "../../../Compositor.hpp"

#include <algorithm>
#include <string_view>

using namespace Config::Lua;

static constexpr const char* MT = "HL.Workspace";

//
static int workspaceEq(lua_State* L) {
    const auto* lhs = sc<PHLWORKSPACEREF*>(luaL_checkudata(L, 1, MT));
    const auto* rhs = sc<PHLWORKSPACEREF*>(luaL_checkudata(L, 2, MT));

    lua_pushboolean(L, lhs->lock() == rhs->lock());
    return 1;
}

static int workspaceToString(lua_State* L) {
    const auto* ref = sc<PHLWORKSPACEREF*>(luaL_checkudata(L, 1, MT));
    const auto  ws  = ref->lock();

    if (!ws || ws->inert())
        lua_pushstring(L, "HL.Workspace(expired)");
    else
        lua_pushfstring(L, "HL.Workspace(%d:%s)", ws->m_id, ws->m_name.c_str());

    return 1;
}

static int workspaceGetWindows(lua_State* L) {
    auto*      ref = sc<PHLWORKSPACEREF*>(luaL_checkudata(L, 1, MT));
    const auto ws  = ref->lock();
    if (!ws || ws->inert()) {
        lua_newtable(L);
        return 1;
    }

    lua_newtable(L);
    int idx = 1;
    for (auto const& w : g_pCompositor->m_windows) {
        if (w->m_workspace == ws) {
            Objects::CLuaWindow::push(L, w);
            lua_rawseti(L, -2, idx++);
        }
    }
    return 1;
}

static int workspaceGetGroups(lua_State* L) {
    auto*      ref = sc<PHLWORKSPACEREF*>(luaL_checkudata(L, 1, MT));
    const auto ws  = ref->lock();
    if (!ws || ws->inert()) {
        lua_newtable(L);
        return 1;
    }

    lua_newtable(L);
    int                                 idx = 1;

    std::vector<Desktop::View::CGroup*> pushedGroups;

    for (auto const& w : g_pCompositor->m_windows) {
        if (w->m_workspace != ws || !w->m_group)
            continue;

        if (std::ranges::find(pushedGroups, w->m_group.get()) != pushedGroups.end())
            continue;

        pushedGroups.push_back(w->m_group.get());

        Objects::CLuaGroup::push(L, w->m_group);

        lua_rawseti(L, -2, idx++);
    }

    return 1;
}

static int workspaceIndex(lua_State* L) {
    auto*      ref = sc<PHLWORKSPACEREF*>(luaL_checkudata(L, 1, MT));
    const auto ws  = ref->lock();
    if (!ws || ws->inert()) {
        Log::logger->log(Log::DEBUG, "[lua] Tried to access an expired object");
        lua_pushnil(L);
        return 1;
    }

    const std::string_view key = luaL_checkstring(L, 2);

    if (key == "id")
        lua_pushinteger(L, sc<lua_Integer>(ws->m_id));
    else if (key == "name")
        lua_pushstring(L, ws->m_name.c_str());
    else if (key == "monitor") {
        const auto mon = ws->m_monitor.lock();
        if (mon)
            Objects::CLuaMonitor::push(L, mon);
        else
            lua_pushnil(L);
    } else if (key == "windows")
        lua_pushinteger(L, sc<lua_Integer>(ws->getWindows()));
    else if (key == "visible")
        lua_pushboolean(L, ws->isVisible());
    else if (key == "special")
        lua_pushboolean(L, ws->m_isSpecialWorkspace);
    else if (key == "active") {
        const auto mon = ws->m_monitor.lock();
        lua_pushboolean(L, mon && (mon->m_activeWorkspace == ws || mon->m_activeSpecialWorkspace == ws));
    } else if (key == "has_urgent")
        lua_pushboolean(L, ws->hasUrgentWindow());
    else if (key == "fullscreen_mode")
        lua_pushinteger(L, sc<lua_Integer>(ws->m_fullscreenMode));
    else if (key == "has_fullscreen")
        lua_pushboolean(L, ws->m_hasFullscreenWindow);
    else if (key == "is_persistent")
        lua_pushboolean(L, ws->isPersistent());
    else if (key == "is_empty")
        lua_pushboolean(L, ws->getWindows() == 0);
    else if (key == "config_name")
        lua_pushstring(L, ws->getConfigName().c_str());
    else if (key == "tiled_layout") {
        std::string layoutName = "unknown";
        if (ws->m_space && ws->m_space->algorithm() && ws->m_space->algorithm()->tiledAlgo()) {
            const auto& TILED_ALGO = ws->m_space->algorithm()->tiledAlgo();
            layoutName             = Layout::Supplementary::algoMatcher()->getNameForTiledAlgo(TILED_ALGO.get());
        }
        lua_pushstring(L, layoutName.c_str());
    } else if (key == "last_window") {
        const auto lastWindow = ws->m_lastFocusedWindow.lock();
        if (lastWindow)
            Objects::CLuaWindow::push(L, lastWindow);
        else
            lua_pushnil(L);
    } else if (key == "fullscreen_window") {
        const auto fsWindow = ws->getFullscreenWindow();
        if (fsWindow)
            Objects::CLuaWindow::push(L, fsWindow);
        else
            lua_pushnil(L);
    } else if (key == "get_windows")
        lua_pushcfunction(L, workspaceGetWindows);
    else if (key == "get_groups")
        lua_pushcfunction(L, workspaceGetGroups);
    else if (key == "groups")
        lua_pushinteger(L, sc<lua_Integer>(ws->getGroups()));
    else
        lua_pushnil(L);

    return 1;
}

void Objects::CLuaWorkspace::setup(lua_State* L) {
    registerMetatable(L, MT, workspaceIndex, gcRef<PHLWORKSPACEREF>, workspaceEq, workspaceToString);
}

void Objects::CLuaWorkspace::push(lua_State* L, PHLWORKSPACE ws) {
    new (lua_newuserdata(L, sizeof(PHLWORKSPACEREF))) PHLWORKSPACEREF(ws ? ws->m_self : nullptr);
    luaL_getmetatable(L, MT);
    lua_setmetatable(L, -2);
}
