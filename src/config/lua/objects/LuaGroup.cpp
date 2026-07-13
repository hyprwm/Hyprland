#include "LuaGroup.hpp"
#include "LuaWindow.hpp"
#include "LuaObjectHelpers.hpp"

#include "../../../desktop/view/Group.hpp"
#include "../../../desktop/view/Window.hpp"

#include <optional>
#include <string_view>

using namespace Config::Lua;

static constexpr const char*     MT        = "HL.Group";
static constexpr const char*     WINDOW_MT = "HL.Window";

static SP<Desktop::View::CGroup> groupFromSelf(lua_State* L, const char* fnName) {
    auto*      ref   = sc<WP<Desktop::View::CGroup>*>(luaL_checkudata(L, 1, MT));
    const auto group = ref->lock();
    if (!group)
        Config::Lua::Bindings::Internal::configError(L, "{}: group object is expired", fnName);

    return group;
}

static PHLWINDOW windowFromArg(lua_State* L, int idx, const char* fnName) {
    auto* ref = sc<PHLWINDOWREF*>(luaL_testudata(L, idx, WINDOW_MT));
    if (!ref) {
        Config::Lua::Bindings::Internal::configError(L, "{}: expected a window object", fnName);
        return nullptr;
    }

    const auto window = ref->lock();
    if (!window)
        Config::Lua::Bindings::Internal::configError(L, "{}: window object is expired", fnName);

    return window;
}

static std::optional<size_t> indexFromArg(lua_State* L, int idx, size_t max, const char* fnName) {
    if (lua_gettop(L) < idx || lua_isnil(L, idx))
        return std::nullopt;

    if (!lua_isinteger(L, idx)) {
        Config::Lua::Bindings::Internal::configError(L, "{}: index must be an integer", fnName);
        return std::nullopt;
    }

    const auto index = lua_tointeger(L, idx);
    if (index < 1 || sc<size_t>(index) > max) {
        Config::Lua::Bindings::Internal::configError(L, "{}: index out of range", fnName);
        return std::nullopt;
    }

    return sc<size_t>(index - 1);
}

static int groupEq(lua_State* L) {
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

static int groupAdd(lua_State* L) {
    constexpr const char* FN = "HL.Group:add";

    const auto            group = groupFromSelf(L, FN);
    if (!group)
        return 0;

    const auto window = windowFromArg(L, 2, FN);
    if (!window)
        return 0;

    std::optional<size_t> index;
    if (lua_gettop(L) >= 3) {
        index = indexFromArg(L, 3, group->size() + 1, FN);
        if (!index && !lua_isnil(L, 3))
            return 0;
    }

    if (window->m_group == group)
        return 0;

    if (group->denied())
        return Config::Lua::Bindings::Internal::configError(L, "{}: target group is denied", FN);

    if (!window->canBeGroupedInto(group))
        return Config::Lua::Bindings::Internal::configError(L, "{}: window cannot be added to group", FN);

    group->add(window, index);

    return 0;
}

static int groupRemove(lua_State* L) {
    constexpr const char* FN = "HL.Group:remove";

    const auto            group = groupFromSelf(L, FN);
    if (!group)
        return 0;

    PHLWINDOW window;
    if (lua_isinteger(L, 2)) {
        const auto index = indexFromArg(L, 2, group->size(), FN);
        if (!index)
            return 0;

        window = group->fromIndex(*index);
    } else
        window = windowFromArg(L, 2, FN);

    if (!window)
        return 0;

    if (!group->has(window))
        return Config::Lua::Bindings::Internal::configError(L, "{}: window is not a group member", FN);

    group->remove(window);

    return 0;
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

    if (key == "add")
        lua_pushcfunction(L, groupAdd);
    else if (key == "remove")
        lua_pushcfunction(L, groupRemove);
    else if (key == "locked")
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
