#include "LuaGroup.hpp"
#include "LuaWindow.hpp"
#include "LuaObjectHelpers.hpp"

#include "../../../desktop/view/Group.hpp"

#include <string_view>

using namespace Config::Lua;

static constexpr const char* MT = "HL.Group";

static int                   groupEq(lua_State* L) {
    const auto* lhs = sc<WP<Desktop::View::CGroup>*>(luaL_checkudata(L, 1, MT));
    const auto* rhs = sc<WP<Desktop::View::CGroup>*>(luaL_checkudata(L, 2, MT));

    lua_pushboolean(L, lhs->lock() == rhs->lock());
    return 1;
}

static int groupToString(lua_State* L) {
    const auto* ref   = sc<WP<Desktop::View::CGroup>*>(luaL_checkudata(L, 1, MT));
    const auto  group = ref->lock();

    if (!group)
        lua_pushstring(L, "HL.Group(expired)");
    else
        lua_pushfstring(L, "HL.Group(%p)", group.get());

    return 1;
}

static int groupIndex(lua_State* L) {
    auto*      ref   = sc<WP<Desktop::View::CGroup>*>(luaL_checkudata(L, 1, MT));
    const auto group = ref->lock();
    if (!group) {
        Log::logger->log(Log::DEBUG, "[lua] Tried to access an expired object");
        lua_pushnil(L);
        return 1;
    }

    const std::string_view key = luaL_checkstring(L, 2);

    if (key == "locked")
        lua_pushboolean(L, group->locked());
    else if (key == "denied")
        lua_pushboolean(L, group->denied());
    else if (key == "size")
        lua_pushinteger(L, sc<lua_Integer>(group->size()));
    else if (key == "current_index")
        lua_pushinteger(L, sc<lua_Integer>(group->getCurrentIdx()) + 1);
    else if (key == "current") {
        const auto current = group->current();
        if (current)
            Objects::CLuaWindow::push(L, current);
        else
            lua_pushnil(L);
    } else if (key == "members") {
        lua_newtable(L);
        int i = 1;
        for (const auto& grouped : group->windows()) {
            const auto groupedWindow = grouped.lock();
            if (!groupedWindow)
                continue;

            Objects::CLuaWindow::push(L, groupedWindow);
            lua_rawseti(L, -2, i++);
        }
    } else
        lua_pushnil(L);

    return 1;
}

void Objects::CLuaGroup::setup(lua_State* L) {
    registerMetatable(L, MT, groupIndex, gcRef<WP<Desktop::View::CGroup>>, groupEq, groupToString);
}

void Objects::CLuaGroup::push(lua_State* L, SP<Desktop::View::CGroup> group) {
    new (lua_newuserdata(L, sizeof(WP<Desktop::View::CGroup>))) WP<Desktop::View::CGroup>(group);
    luaL_getmetatable(L, MT);
    lua_setmetatable(L, -2);
}
