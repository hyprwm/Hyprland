#include "LuaKeybind.hpp"

#include <string_view>

using namespace Config::Lua;

static constexpr const char* MT = "HL.Keybind";

static Keybinds::PBind       getKeybindFromUserdata(lua_State* L) {
    auto* ref = sc<WP<Keybinds::CBind>*>(luaL_checkudata(L, 1, MT));
    return ref->lock();
}

static void pushDeviceList(lua_State* L, const Keybinds::CBind& keybind) {
    lua_newtable(L);
    int i = 1;
    for (const auto& device : keybind.devices()) {
        lua_pushstring(L, device.c_str());
        lua_rawseti(L, -2, i++);
    }
}

static std::string_view keyName(const Keybinds::CBind& keybind) {
    const auto KEYS      = keybind.keys();
    const auto KEY_NAMES = keybind.keyNames();
    if (keybind.hasFlag(Keybinds::BIND_FLAG_CATCH_ALL) || KEYS.empty() || KEYS.back().keycode() || KEY_NAMES.empty())
        return {};

    return KEY_NAMES.back();
}

static xkb_keycode_t keycode(const Keybinds::CBind& keybind) {
    const auto KEYS = keybind.keys();
    if (KEYS.empty())
        return 0;

    return KEYS.back().keycode().value_or(0);
}

static int keybindEq(lua_State* L) {
    const auto* lhs = sc<WP<Keybinds::CBind>*>(luaL_checkudata(L, 1, MT));
    const auto* rhs = sc<WP<Keybinds::CBind>*>(luaL_checkudata(L, 2, MT));

    lua_pushboolean(L, lhs->lock() == rhs->lock());
    return 1;
}

static int keybindToString(lua_State* L) {
    const auto* ref     = sc<WP<Keybinds::CBind>*>(luaL_checkudata(L, 1, MT));
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

    keybind->setEnabled(lua_toboolean(L, 2));
    return 0;
}

static int keybindIsEnabled(lua_State* L) {
    const auto keybind = getKeybindFromUserdata(L);
    if (!keybind) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushboolean(L, keybind->enabled());
    return 1;
}

static int keybindRemove(lua_State* L) {
    const auto keybind = getKeybindFromUserdata(L);
    if (!keybind || !Keybinds::mgr())
        return 0;

    Keybinds::mgr()->removeBind(keybind);
    return 0;
}

static int keybindGetDescription(lua_State* L) {
    const auto keybind = getKeybindFromUserdata(L);
    if (!keybind) {
        lua_pushnil(L);
        return 1;
    }

    const auto& DESCRIPTION = keybind->metadata().description;
    if (!DESCRIPTION)
        lua_pushnil(L);
    else
        lua_pushstring(L, DESCRIPTION->c_str());

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
        lua_pushboolean(L, keybind->enabled());
    else if (key == "has_description")
        lua_pushboolean(L, keybind->metadata().description.has_value());
    else if (key == "description")
        return keybindGetDescription(L);
    else if (key == "display_key")
        lua_pushstring(L, keybind->metadata().displayKey.c_str());
    else if (key == "submap")
        lua_pushstring(L, keybind->metadata().submap.c_str());
    else if (key == "handler")
        lua_pushstring(L, keybind->metadata().handler.c_str());
    else if (key == "arg")
        lua_pushstring(L, keybind->metadata().argument.c_str());
    else if (key == "modmask")
        lua_pushinteger(L, sc<lua_Integer>(keybind->modifierMask()));
    else if (key == "key") {
        const auto KEY_NAME = keyName(*keybind);
        if (KEY_NAME.empty())
            lua_pushliteral(L, "");
        else
            lua_pushlstring(L, KEY_NAME.data(), KEY_NAME.size());
    } else if (key == "keycode")
        lua_pushinteger(L, sc<lua_Integer>(keycode(*keybind)));
    else if (key == "catchall")
        lua_pushboolean(L, keybind->hasFlag(Keybinds::BIND_FLAG_CATCH_ALL));
    else if (key == "repeating")
        lua_pushboolean(L, keybind->hasFlag(Keybinds::BIND_FLAG_REPEAT));
    else if (key == "locked")
        lua_pushboolean(L, keybind->hasFlag(Keybinds::BIND_FLAG_LOCKED));
    else if (key == "release")
        lua_pushboolean(L, keybind->hasFlag(Keybinds::BIND_FLAG_RELEASE));
    else if (key == "non_consuming")
        lua_pushboolean(L, keybind->hasFlag(Keybinds::BIND_FLAG_NON_CONSUMING));
    else if (key == "auto_consuming")
        lua_pushboolean(L, keybind->hasFlag(Keybinds::BIND_FLAG_AUTO_CONSUMING));
    else if (key == "transparent")
        lua_pushboolean(L, keybind->hasFlag(Keybinds::BIND_FLAG_TRANSPARENT));
    else if (key == "ignore_mods")
        lua_pushboolean(L, keybind->hasFlag(Keybinds::BIND_FLAG_IGNORE_MODS));
    else if (key == "long_press")
        lua_pushboolean(L, keybind->hasFlag(Keybinds::BIND_FLAG_LONG_PRESS));
    else if (key == "dont_inhibit")
        lua_pushboolean(L, keybind->hasFlag(Keybinds::BIND_FLAG_DONT_INHIBIT));
    else if (key == "click")
        lua_pushboolean(L, keybind->hasFlag(Keybinds::BIND_FLAG_CLICK));
    else if (key == "drag")
        lua_pushboolean(L, keybind->hasFlag(Keybinds::BIND_FLAG_DRAG));
    else if (key == "submap_universal")
        lua_pushboolean(L, keybind->hasFlag(Keybinds::BIND_FLAG_SUBMAP_UNIVERSAL));
    else if (key == "mouse")
        lua_pushboolean(L, keybind->hasFlag(Keybinds::BIND_FLAG_MOUSE));
    else if (key == "device_inclusive")
        lua_pushboolean(L, keybind->hasFlag(Keybinds::BIND_FLAG_DEVICE_INCLUSIVE));
    else if (key == "devices")
        pushDeviceList(L, *keybind);
    else if (key == "allow_input_capture")
        lua_pushboolean(L, keybind->hasFlag(Keybinds::BIND_FLAG_ALLOW_INPUT_CAPTURE));
    else
        lua_pushnil(L);

    return 1;
}

void Objects::CLuaKeybind::setup(lua_State* L) {
    registerMetatable(L, MT, keybindIndex, gcRef<WP<Keybinds::CBind>>, keybindEq, keybindToString);
}

void Objects::CLuaKeybind::push(lua_State* L, const Keybinds::PBind& keybind) {
    new (lua_newuserdata(L, sizeof(WP<Keybinds::CBind>))) WP<Keybinds::CBind>(keybind);
    luaL_getmetatable(L, MT);
    lua_setmetatable(L, -2);
}
