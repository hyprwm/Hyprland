#include "LuaKeybind.hpp"

#include <optional>
#include <string_view>

using namespace Config::Lua;

static constexpr const char* MT = "HL.Keybind";

namespace {
    std::optional<SP<SKeybind>> getKeybindFromUserdata(lua_State* L) {
        auto* ref = sc<WP<SKeybind>*>(luaL_checkudata(L, 1, MT));
        return ref->lock();
    }

    void pushDeviceList(lua_State* L, const SKeybind& keybind) {
        lua_newtable(L);
        int i = 1;
        for (const auto& device : keybind.devices) {
            lua_pushstring(L, device.c_str());
            lua_rawseti(L, -2, i++);
        }
    }
}

static int keybindEq(lua_State* L) {
    const auto* lhs = sc<WP<SKeybind>*>(luaL_checkudata(L, 1, MT));
    const auto* rhs = sc<WP<SKeybind>*>(luaL_checkudata(L, 2, MT));

    lua_pushboolean(L, lhs->lock() == rhs->lock());
    return 1;
}

static int keybindToString(lua_State* L) {
    const auto* ref     = sc<WP<SKeybind>*>(luaL_checkudata(L, 1, MT));
    const auto  keybind = ref->lock();

    if (!keybind)
        lua_pushstring(L, "HL.Keybind(expired)");
    else
        lua_pushfstring(L, "HL.Keybind(%p)", keybind.get());

    return 1;
}

static int keybindSetEnabled(lua_State* L) {
    luaL_checktype(L, 2, LUA_TBOOLEAN);

    const auto keybind = getKeybindFromUserdata(L);
    if (!keybind)
        return 0;

    (*keybind)->enabled = lua_toboolean(L, 2);
    return 0;
}

static int keybindIsEnabled(lua_State* L) {
    const auto keybind = getKeybindFromUserdata(L);
    if (!keybind) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushboolean(L, (*keybind)->enabled);
    return 1;
}

static int keybindRemove(lua_State* L) {
    const auto keybind = getKeybindFromUserdata(L);
    if (!keybind || !g_pKeybindManager)
        return 0;

    if ((*keybind)->handler == "__lua") {
        try {
            const int ref = std::stoi((*keybind)->arg);
            if (ref > 0)
                luaL_unref(L, LUA_REGISTRYINDEX, ref);
        } catch (...) {
            // invalid ref, ignore
        }

        (*keybind)->arg = std::to_string(LUA_NOREF);
    }

    g_pKeybindManager->removeKeybind((*keybind)->modmask, SParsedKey{.key = (*keybind)->key, .keycode = (*keybind)->keycode, .catchAll = (*keybind)->catchAll});
    return 0;
}

static int keybindGetDescription(lua_State* L) {
    const auto keybind = getKeybindFromUserdata(L);
    if (!keybind) {
        lua_pushnil(L);
        return 1;
    }

    if (!(*keybind)->hasDescription)
        lua_pushnil(L);
    else
        lua_pushstring(L, (*keybind)->description.c_str());

    return 1;
}

static int keybindIndex(lua_State* L) {
    const auto             keybind = getKeybindFromUserdata(L);
    const std::string_view key     = luaL_checkstring(L, 2);

    if (key == "set_enabled")
        lua_pushcfunction(L, keybindSetEnabled);
    else if (key == "is_enabled")
        lua_pushcfunction(L, keybindIsEnabled);
    else if (key == "remove" || key == "unbind")
        lua_pushcfunction(L, keybindRemove);
    else if (!keybind)
        lua_pushnil(L);
    else if (key == "enabled")
        lua_pushboolean(L, (*keybind)->enabled);
    else if (key == "has_description")
        lua_pushboolean(L, (*keybind)->hasDescription);
    else if (key == "description")
        return keybindGetDescription(L);
    else if (key == "display_key")
        lua_pushstring(L, (*keybind)->displayKey.c_str());
    else if (key == "submap")
        lua_pushstring(L, (*keybind)->submap.name.c_str());
    else if (key == "handler")
        lua_pushstring(L, (*keybind)->handler.c_str());
    else if (key == "arg")
        lua_pushstring(L, (*keybind)->arg.c_str());
    else if (key == "modmask")
        lua_pushinteger(L, sc<lua_Integer>((*keybind)->modmask));
    else if (key == "key")
        lua_pushstring(L, (*keybind)->key.c_str());
    else if (key == "keycode")
        lua_pushinteger(L, sc<lua_Integer>((*keybind)->keycode));
    else if (key == "catchall")
        lua_pushboolean(L, (*keybind)->catchAll);
    else if (key == "repeating")
        lua_pushboolean(L, (*keybind)->repeat);
    else if (key == "locked")
        lua_pushboolean(L, (*keybind)->locked);
    else if (key == "release")
        lua_pushboolean(L, (*keybind)->release);
    else if (key == "non_consuming")
        lua_pushboolean(L, (*keybind)->nonConsuming);
    else if (key == "transparent")
        lua_pushboolean(L, (*keybind)->transparent);
    else if (key == "ignore_mods")
        lua_pushboolean(L, (*keybind)->ignoreMods);
    else if (key == "long_press")
        lua_pushboolean(L, (*keybind)->longPress);
    else if (key == "dont_inhibit")
        lua_pushboolean(L, (*keybind)->dontInhibit);
    else if (key == "click")
        lua_pushboolean(L, (*keybind)->click);
    else if (key == "drag")
        lua_pushboolean(L, (*keybind)->drag);
    else if (key == "submap_universal")
        lua_pushboolean(L, (*keybind)->submapUniversal);
    else if (key == "mouse")
        lua_pushboolean(L, (*keybind)->mouse);
    else if (key == "device_inclusive")
        lua_pushboolean(L, (*keybind)->deviceInclusive);
    else if (key == "devices")
        pushDeviceList(L, **keybind);
    else
        lua_pushnil(L);

    return 1;
}

void Objects::CLuaKeybind::setup(lua_State* L) {
    registerMetatable(L, MT, keybindIndex, gcRef<WP<SKeybind>>, keybindEq, keybindToString);
}

void Objects::CLuaKeybind::push(lua_State* L, const SP<SKeybind>& keybind) {
    new (lua_newuserdata(L, sizeof(WP<SKeybind>))) WP<SKeybind>(keybind);
    luaL_getmetatable(L, MT);
    lua_setmetatable(L, -2);
}
