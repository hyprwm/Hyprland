#include "LuaDevice.hpp"

#include "../../../devices/IKeyboard.hpp"
#include "../../../devices/IPointer.hpp"
#include "../../../devices/Tablet.hpp"

#include <aquamarine/input/Input.hpp>
#include <format>
#include <libinput.h>
#include <limits>
#include <string_view>
#include <variant>
#include <xkbcommon/xkbcommon-names.h>

using namespace Config::Lua;

static constexpr const char* MT = "HL.Device";

struct SDeviceRef {
    std::variant<WP<IHID>, WP<Aquamarine::ISwitch>> device;
    uintptr_t                                       address = 0;
    bool                                            removed = false;
    CHyprSignalListener                             destroy;
};

static const char* deviceType(eHIDType type) {
    switch (type) {
        case HID_TYPE_POINTER: return "pointer";
        case HID_TYPE_KEYBOARD: return "keyboard";
        case HID_TYPE_TOUCH: return "touch";
        case HID_TYPE_TABLET: return "tablet";
        case HID_TYPE_TABLET_PAD: return "tablet_pad";
        case HID_TYPE_TABLET_TOOL: return "tablet_tool";
        default: return nullptr;
    }
}

static bool deviceAvailable(const SDeviceRef& ref) {
    return !ref.removed && std::visit([](const auto& device) { return bool(device.lock()); }, ref.device);
}

static void pushAddress(lua_State* L, uintptr_t address) {
    lua_pushstring(L, std::format("0x{:x}", address).c_str());
}

static void pushPointerProperty(lua_State* L, IPointer& pointer, std::string_view key) {
    if (key == "default_speed") {
        const auto backend = pointer.aq();
        const auto handle  = backend ? backend->getLibinputHandle() : nullptr;
        lua_pushnumber(L, handle ? libinput_device_config_accel_get_default_speed(handle) : 0.0);
    } else if (key == "scroll_factor")
        lua_pushnumber(L, pointer.m_scrollFactor.value_or(-1));
    else
        lua_pushnil(L);
}

static void pushKeyboardProperty(lua_State* L, IKeyboard& keyboard, std::string_view key) {
    if (key == "rules")
        lua_pushstring(L, keyboard.m_currentRules.rules.c_str());
    else if (key == "model")
        lua_pushstring(L, keyboard.m_currentRules.model.c_str());
    else if (key == "layout")
        lua_pushstring(L, keyboard.m_currentRules.layout.c_str());
    else if (key == "variant")
        lua_pushstring(L, keyboard.m_currentRules.variant.c_str());
    else if (key == "options")
        lua_pushstring(L, keyboard.m_currentRules.options.c_str());
    else if (key == "main")
        lua_pushboolean(L, keyboard.m_active);
    else if (!keyboard.m_xkbKeymap || !keyboard.m_xkbState)
        lua_pushnil(L);
    else if (key == "active_layout_index") {
        const auto index = keyboard.getActiveLayoutIndex();
        if (index)
            lua_pushinteger(L, *index);
        else
            lua_pushnil(L);
    } else if (key == "active_keymap")
        lua_pushstring(L, keyboard.getActiveLayout().c_str());
    else if (key == "caps_lock" || key == "num_lock") {
        const auto index = xkb_keymap_mod_get_index(keyboard.m_xkbKeymap, key == "caps_lock" ? XKB_MOD_NAME_CAPS : XKB_MOD_NAME_NUM);
        lua_pushboolean(L, index < std::numeric_limits<decltype(keyboard.m_modifiersState.locked)>::digits && (keyboard.m_modifiersState.locked & (uint32_t{1} << index)));
    } else
        lua_pushnil(L);
}

static void pushTabletPadProperty(lua_State* L, CTabletPad& pad, std::string_view key) {
    const auto parent = pad.m_parent.lock();
    if (key != "belongs_to" || !parent) {
        lua_pushnil(L);
        return;
    }

    lua_newtable(L);
    pushAddress(L, rc<uintptr_t>(parent.get()));
    lua_setfield(L, -2, "address");
    lua_pushstring(L, parent->m_hlName.c_str());
    lua_setfield(L, -2, "name");
}

static int deviceIndex(lua_State* L) {
    const auto& ref = *sc<SDeviceRef*>(luaL_checkudata(L, 1, MT));
    if (!deviceAvailable(ref)) {
        lua_pushnil(L);
        return 1;
    }

    const std::string_view key = luaL_checkstring(L, 2);
    if (key == "address") {
        pushAddress(L, ref.address);
        return 1;
    }

    if (const auto switchRef = std::get_if<WP<Aquamarine::ISwitch>>(&ref.device)) {
        const auto device = switchRef->lock();
        if (key == "type")
            lua_pushstring(L, "switch");
        else if (key == "name")
            lua_pushstring(L, device->getName().c_str());
        else
            lua_pushnil(L);
        return 1;
    }

    const auto device = std::get<WP<IHID>>(ref.device).lock();
    const auto type   = device->getType();
    if (key == "type")
        lua_pushstring(L, deviceType(type));
    else if (key == "name" && type != HID_TYPE_TABLET_PAD && type != HID_TYPE_TABLET_TOOL)
        lua_pushstring(L, device->m_hlName.c_str());
    else {
        switch (type) {
            case HID_TYPE_POINTER: pushPointerProperty(L, sc<IPointer&>(*device), key); break;
            case HID_TYPE_KEYBOARD: pushKeyboardProperty(L, sc<IKeyboard&>(*device), key); break;
            case HID_TYPE_TABLET_PAD: pushTabletPadProperty(L, sc<CTabletPad&>(*device), key); break;
            default: lua_pushnil(L); break;
        }
    }

    return 1;
}

static int deviceEq(lua_State* L) {
    const auto& lhs = *sc<SDeviceRef*>(luaL_checkudata(L, 1, MT));
    const auto& rhs = *sc<SDeviceRef*>(luaL_checkudata(L, 2, MT));
    lua_pushboolean(L, lhs.device == rhs.device);
    return 1;
}

static int deviceToString(lua_State* L) {
    const auto& ref = *sc<SDeviceRef*>(luaL_checkudata(L, 1, MT));
    lua_pushstring(L, deviceAvailable(ref) ? std::format("HL.Device(0x{:x})", ref.address).c_str() : "HL.Device(expired)");
    return 1;
}

void Objects::CLuaDevice::setup(lua_State* L) {
    registerMetatable(L, MT, deviceIndex, gcRef<SDeviceRef>, deviceEq, deviceToString);
}

void Objects::CLuaDevice::push(lua_State* L, WP<IHID> device) {
    const auto locked = device.lock();
    if (!locked) {
        lua_pushnil(L);
        return;
    }

    auto* ref = new (lua_newuserdata(L, sizeof(SDeviceRef))) SDeviceRef{
        .device  = device,
        .address = rc<uintptr_t>(locked.get()),
    };
    ref->destroy = locked->m_events.destroy.listen([ref] { ref->removed = true; });
    luaL_getmetatable(L, MT);
    lua_setmetatable(L, -2);
}

void Objects::CLuaDevice::push(lua_State* L, WP<Aquamarine::ISwitch> device, uintptr_t address) {
    const auto locked = device.lock();
    if (!locked) {
        lua_pushnil(L);
        return;
    }

    auto* ref = new (lua_newuserdata(L, sizeof(SDeviceRef))) SDeviceRef{
        .device  = device,
        .address = address,
    };
    ref->destroy = locked->events.destroy.listen([ref] { ref->removed = true; });
    luaL_getmetatable(L, MT);
    lua_setmetatable(L, -2);
}
