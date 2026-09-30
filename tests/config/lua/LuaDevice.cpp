#include <Compositor.hpp>
#include <config/lua/bindings/LuaBindingsInternal.hpp>
#include <config/lua/objects/LuaDevice.hpp>
#include <devices/IKeyboard.hpp>
#include <devices/Mouse.hpp>
#include <devices/Tablet.hpp>
#include <devices/TouchDevice.hpp>
#include <managers/input/InputManager.hpp>
#include <protocols/Tablet.hpp>

#include <aquamarine/input/Input.hpp>
#include <format>
#include <gtest/gtest.h>
#include <hyprutils/utils/ScopeGuard.hpp>
#include <xkbcommon/xkbcommon-names.h>

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

using namespace Config::Lua;
using namespace Hyprutils::Utils;

class CLuaDeviceKeyboard : public IKeyboard {
  public:
    bool isVirtual() const override {
        return false;
    }

    SP<Aquamarine::IKeyboard> aq() override {
        return nullptr;
    }
};

class CLuaDeviceSwitch : public Aquamarine::ISwitch {
  public:
    const std::string& getName() override {
        return m_name;
    }

    std::string m_name = "test-switch";
};

class CConfigLuaDevice : public testing::Test {
  protected:
    void SetUp() override {
        m_lua = luaL_newstate();
        ASSERT_NE(m_lua, nullptr);
        luaL_openlibs(m_lua);
        Objects::CLuaDevice{}.setup(m_lua);
        ASSERT_EQ(luaL_dostring(m_lua, R"(
            function assertExpired(device)
                for _, field in ipairs({
                    'type', 'address', 'name', 'default_speed', 'scroll_factor',
                    'rules', 'model', 'layout', 'variant', 'options', 'main',
                    'active_layout_index', 'active_keymap', 'caps_lock', 'num_lock', 'belongs_to'
                }) do
                    assert(device[field] == nil, field .. ' survived device destruction')
                end
                assert(tostring(device):match('^HL%.Device%('))
            end
        )"),
                  LUA_OK)
            << lua_tostring(m_lua, -1);
    }

    void TearDown() override {
        if (m_lua)
            lua_close(m_lua);
    }

    void push(WP<IHID> device, const char* name = "device") {
        Objects::CLuaDevice::push(m_lua, device);
        lua_setglobal(m_lua, name);
    }

    lua_State* m_lua = nullptr;
};

TEST_F(CConfigLuaDevice, pointerReadsLiveValuesWithoutBackend) {
    const auto mouse    = CMouse::create(nullptr);
    mouse->m_deviceName = "backend-name";
    mouse->m_hlName     = "test-pointer";
    push(mouse);

    const auto ADDRESS = std::format("0x{:x}", rc<uintptr_t>(mouse.get()));
    lua_pushstring(m_lua, ADDRESS.c_str());
    lua_setglobal(m_lua, "expectedAddress");
    ASSERT_EQ(luaL_dostring(m_lua, R"(
        assert(type(device) == 'userdata')
        assert(device.type == 'pointer')
        assert(device.address == expectedAddress)
        assert(device.name == 'test-pointer')
        assert(device.default_speed == 0)
        assert(device.scroll_factor == -1)
        assert(device.layout == nil and device.belongs_to == nil)
        assert(device.unknown == nil)
        assert(tostring(device):match('^HL%.Device%('))
    )"),
              LUA_OK)
        << lua_tostring(m_lua, -1);

    mouse->m_hlName       = "renamed-pointer";
    mouse->m_scrollFactor = 2.5F;
    ASSERT_EQ(luaL_dostring(m_lua, "assert(device.name == 'renamed-pointer' and device.scroll_factor == 2.5)"), LUA_OK) << lua_tostring(m_lua, -1);
    mouse->m_scrollFactor.reset();
    ASSERT_EQ(luaL_dostring(m_lua, "assert(device.scroll_factor == -1)"), LUA_OK) << lua_tostring(m_lua, -1);
}

TEST_F(CConfigLuaDevice, keyboardRulesAreLiveWithoutXkb) {
    const auto keyboard      = makeShared<CLuaDeviceKeyboard>();
    keyboard->m_hlName       = "test-keyboard";
    keyboard->m_currentRules = {
        .layout  = "us,pl",
        .model   = "pc105",
        .variant = ",",
        .options = "grp:alt_shift_toggle",
        .rules   = "evdev",
    };
    push(keyboard);

    ASSERT_EQ(luaL_dostring(m_lua, R"(
        assert(device.type == 'keyboard' and device.name == 'test-keyboard')
        assert(type(device.address) == 'string')
        assert(device.rules == 'evdev' and device.model == 'pc105')
        assert(device.layout == 'us,pl' and device.variant == ',')
        assert(device.options == 'grp:alt_shift_toggle')
        assert(device.main == false)
        assert(device.active_layout_index == nil and device.active_keymap == nil)
        assert(device.caps_lock == nil and device.num_lock == nil)
        assert(device.default_speed == nil and device.scroll_factor == nil)
    )"),
              LUA_OK)
        << lua_tostring(m_lua, -1);

    keyboard->m_active       = true;
    keyboard->m_currentRules = {
        .layout  = "de",
        .model   = "pc104",
        .variant = "nodeadkeys",
        .options = "",
        .rules   = "base",
    };
    ASSERT_EQ(luaL_dostring(m_lua, R"(
        assert(device.main == true)
        assert(device.rules == 'base' and device.model == 'pc104')
        assert(device.layout == 'de' and device.variant == 'nodeadkeys' and device.options == '')
    )"),
              LUA_OK)
        << lua_tostring(m_lua, -1);
}

TEST_F(CConfigLuaDevice, keyboardLayoutAndLocksAreLive) {
    const auto keyboard = makeShared<CLuaDeviceKeyboard>();
    const auto CONTEXT  = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    ASSERT_NE(CONTEXT, nullptr);
    const CScopeGuard    CONTEXT_GUARD = {[&] { xkb_context_unref(CONTEXT); }};
    const xkb_rule_names RULES         = {
        .rules   = "evdev",
        .model   = "pc105",
        .layout  = "us,pl",
        .variant = "",
        .options = "",
    };
    // IKeyboard owns these allocations; avoid setKeymap's compositor dependencies.
    keyboard->m_xkbKeymap = xkb_keymap_new_from_names(CONTEXT, &RULES, XKB_KEYMAP_COMPILE_NO_FLAGS);
    ASSERT_NE(keyboard->m_xkbKeymap, nullptr);
    push(keyboard);
    ASSERT_EQ(luaL_dostring(m_lua, R"(
        assert(device.active_layout_index == nil and device.active_keymap == nil)
        assert(device.caps_lock == nil and device.num_lock == nil)
    )"),
              LUA_OK)
        << lua_tostring(m_lua, -1);

    keyboard->m_xkbState = xkb_state_new(keyboard->m_xkbKeymap);
    ASSERT_NE(keyboard->m_xkbState, nullptr);
    ASSERT_EQ(luaL_dostring(m_lua, R"(
        assert(device.active_layout_index == 0)
        assert(device.active_keymap == 'English (US)')
        assert(device.caps_lock == false and device.num_lock == false)
    )"),
              LUA_OK)
        << lua_tostring(m_lua, -1);

    const auto CAPS = xkb_keymap_mod_get_index(keyboard->m_xkbKeymap, XKB_MOD_NAME_CAPS);
    const auto NUM  = xkb_keymap_mod_get_index(keyboard->m_xkbKeymap, XKB_MOD_NAME_NUM);
    ASSERT_NE(CAPS, XKB_MOD_INVALID);
    ASSERT_NE(NUM, XKB_MOD_INVALID);
    keyboard->m_modifiersState.locked = (1U << CAPS) | (1U << NUM);
    xkb_state_update_mask(keyboard->m_xkbState, 0, 0, keyboard->m_modifiersState.locked, 0, 0, 1);
    ASSERT_EQ(luaL_dostring(m_lua, R"(
        assert(device.active_layout_index == 1 and device.active_keymap == 'Polish')
        assert(device.caps_lock == true and device.num_lock == true)
    )"),
              LUA_OK)
        << lua_tostring(m_lua, -1);

    keyboard->m_modifiersState.locked = 0;
    xkb_state_update_mask(keyboard->m_xkbState, 0, 0, 0, 0, 0, 0);
    ASSERT_EQ(luaL_dostring(m_lua, R"(
        assert(device.active_layout_index == 0 and device.active_keymap == 'English (US)')
        assert(device.caps_lock == false and device.num_lock == false)
    )"),
              LUA_OK)
        << lua_tostring(m_lua, -1);

    // A state may temporarily outlive the keyboard's keymap reference.
    xkb_keymap_unref(keyboard->m_xkbKeymap);
    keyboard->m_xkbKeymap = nullptr;
    ASSERT_EQ(luaL_dostring(m_lua, R"(
        assert(device.active_layout_index == nil and device.active_keymap == nil)
        assert(device.caps_lock == nil and device.num_lock == nil)
    )"),
              LUA_OK)
        << lua_tostring(m_lua, -1);
    keyboard->m_events.destroy.emit();
    ASSERT_EQ(luaL_dostring(m_lua, "assertExpired(device)"), LUA_OK) << lua_tostring(m_lua, -1);
}

TEST_F(CConfigLuaDevice, writesDoNotThrowOrChangeFields) {
    const auto mouse = CMouse::create(nullptr);
    mouse->m_hlName  = "readonly-pointer";
    push(mouse);
    ASSERT_EQ(luaL_dostring(m_lua, R"(
        assert(pcall(function() device.name = 'changed' end))
        assert(pcall(function() device.scroll_factor = 9 end))
        assert(pcall(function() device.unknown = true end))
        assert(device.name == 'readonly-pointer' and device.scroll_factor == -1)
        assert(device.unknown == nil)
    )"),
              LUA_OK)
        << lua_tostring(m_lua, -1);
    EXPECT_EQ(mouse->m_hlName, "readonly-pointer");
    EXPECT_FALSE(mouse->m_scrollFactor.has_value());
}

TEST_F(CConfigLuaDevice, weakIdentitySurvivesExpiryWithoutKeepingDevicesAlive) {
    auto           first  = CMouse::create(nullptr);
    auto           second = CMouse::create(nullptr);
    const WP<IHID> weak   = first;
    push(first, "first");
    push(first, "alias");
    push(second, "second");
    ASSERT_EQ(luaL_dostring(m_lua, "assert(first == alias and first ~= second)"), LUA_OK) << lua_tostring(m_lua, -1);
    first.reset();
    second.reset();
    EXPECT_TRUE(weak.expired());
    ASSERT_EQ(luaL_dostring(m_lua, R"(
        assertExpired(first)
        assertExpired(alias)
        assertExpired(second)
        assert(first == alias and first ~= second)
    )"),
              LUA_OK)
        << lua_tostring(m_lua, -1);
}

TEST_F(CConfigLuaDevice, destroyInvalidatesAllFieldsWhileStrongReferenceRemains) {
    const auto mouse = CMouse::create(nullptr);
    const auto other = CMouse::create(nullptr);
    push(mouse, "device");
    push(mouse, "alias");
    push(other, "other");
    mouse->m_events.destroy.emit();
    other->m_events.destroy.emit();
    mouse->m_hlName       = "still-alive";
    mouse->m_scrollFactor = 3.F;
    ASSERT_EQ(luaL_dostring(m_lua, R"(
        assertExpired(device)
        assertExpired(alias)
        assertExpired(other)
        assert(device == alias and device ~= other)
    )"),
              LUA_OK)
        << lua_tostring(m_lua, -1);
}

TEST_F(CConfigLuaDevice, touchNameIsLiveAndDestroyInvalidatesIt) {
    const auto touch = CTouchDevice::create(nullptr);
    touch->m_hlName  = "test-touch";
    push(touch);
    ASSERT_EQ(luaL_dostring(m_lua, "assert(device.type == 'touch' and device.name == 'test-touch')"), LUA_OK) << lua_tostring(m_lua, -1);
    touch->m_hlName = "renamed-touch";
    ASSERT_EQ(luaL_dostring(m_lua, "assert(device.name == 'renamed-touch')"), LUA_OK) << lua_tostring(m_lua, -1);
    touch->m_events.destroy.emit();
    ASSERT_EQ(luaL_dostring(m_lua, "assertExpired(device)"), LUA_OK) << lua_tostring(m_lua, -1);
}

TEST_F(CConfigLuaDevice, tabletFamilyPropertiesAndPadParentAreLive) {
    auto              previousCompositor = std::move(g_pCompositor);
    auto              previousProtocol   = std::move(PROTO::tablet);
    const CScopeGuard GLOBALS_GUARD      = {[&] {
        // Device destructors recheck the protocol's registrations, so the protocol
        // must outlive all devices and be removed before destroying its display.
        PROTO::tablet.reset();
        if (g_pCompositor && g_pCompositor->m_wlDisplay) {
            wl_display_destroy(g_pCompositor->m_wlDisplay);
            g_pCompositor->m_wlDisplay = nullptr;
        }
        g_pCompositor = std::move(previousCompositor);
        PROTO::tablet = std::move(previousProtocol);
    }};
    g_pCompositor                        = makeUnique<CCompositor>(true);
    g_pCompositor->m_wlDisplay           = wl_display_create();
    ASSERT_NE(g_pCompositor->m_wlDisplay, nullptr);
    PROTO::tablet = makeUnique<CTabletV2Protocol>(&zwp_tablet_manager_v2_interface, 1, "TabletV2");
    ASSERT_NE(PROTO::tablet->getGlobal(), nullptr);

    const auto tablet    = CTablet::create(nullptr);
    auto       pad       = CTabletPad::create(nullptr);
    auto       tool      = CTabletTool::create(nullptr);
    tablet->m_deviceName = "backend-tablet";
    tablet->m_hlName     = "test-tablet";
    pad->m_hlName        = "hidden-pad-name";
    tool->m_hlName       = "test-tool";
    push(tablet, "tablet");
    push(pad, "pad");
    push(tool, "tool");
    ASSERT_EQ(luaL_dostring(m_lua, R"(
        assert(tablet.type == 'tablet' and tablet.name == 'test-tablet')
        assert(pad.type == 'tablet_pad' and pad.name == nil)
        assert(tool.type == 'tablet_tool' and tool.name == nil)
        for _, device in ipairs({ tablet, pad, tool }) do
            assert(type(device.address) == 'string' and device.address:match('^0x%x+$'))
            assert(device.belongs_to == nil)
            assert(device.default_speed == nil and device.scroll_factor == nil)
        end
    )"),
              LUA_OK)
        << lua_tostring(m_lua, -1);

    tablet->m_hlName          = "renamed-tablet";
    pad->m_parent             = tool;
    const auto PARENT_ADDRESS = std::format("0x{:x}", rc<uintptr_t>(tool.get()));
    lua_pushstring(m_lua, PARENT_ADDRESS.c_str());
    lua_setglobal(m_lua, "expectedParentAddress");
    ASSERT_EQ(luaL_dostring(m_lua, R"(
        assert(tablet.name == 'renamed-tablet')
        parentSnapshot = pad.belongs_to
        assert(type(parentSnapshot) == 'table')
        assert(parentSnapshot.address == expectedParentAddress and parentSnapshot.name == 'test-tool')
        local another = pad.belongs_to
        assert(not rawequal(parentSnapshot, another))
        another.address = 'changed'
        another.name = 'changed'
        assert(pad.belongs_to.address == expectedParentAddress and pad.belongs_to.name == 'test-tool')
    )"),
              LUA_OK)
        << lua_tostring(m_lua, -1);
    EXPECT_EQ(tool->m_hlName, "test-tool");

    tool->m_hlName = "renamed-tool";
    ASSERT_EQ(luaL_dostring(m_lua, R"(
        assert(pad.belongs_to.name == 'renamed-tool')
        assert(parentSnapshot.name == 'test-tool')
        assert(tool.name == nil and pad.name == nil)
    )"),
              LUA_OK)
        << lua_tostring(m_lua, -1);

    auto replacement               = CTabletTool::create(nullptr);
    replacement->m_hlName          = "replacement-tool";
    pad->m_parent                  = replacement;
    const auto REPLACEMENT_ADDRESS = std::format("0x{:x}", rc<uintptr_t>(replacement.get()));
    lua_pushstring(m_lua, REPLACEMENT_ADDRESS.c_str());
    lua_setglobal(m_lua, "expectedReplacementAddress");
    ASSERT_EQ(luaL_dostring(m_lua, R"(
        replacementSnapshot = pad.belongs_to
        assert(replacementSnapshot.address == expectedReplacementAddress)
        assert(replacementSnapshot.address ~= parentSnapshot.address)
        assert(replacementSnapshot.name == 'replacement-tool')
        assert(parentSnapshot.address == expectedParentAddress and parentSnapshot.name == 'test-tool')
    )"),
              LUA_OK)
        << lua_tostring(m_lua, -1);

    tool.reset();
    ASSERT_EQ(luaL_dostring(m_lua, R"(
        assertExpired(tool)
        assert(pad.belongs_to.address == expectedReplacementAddress)
    )"),
              LUA_OK)
        << lua_tostring(m_lua, -1);
    replacement.reset();
    EXPECT_TRUE(pad->m_parent.expired());
    ASSERT_EQ(luaL_dostring(m_lua, R"(
        assert(pad.type == 'tablet_pad' and pad.belongs_to == nil)
        assert(replacementSnapshot.address == expectedReplacementAddress)
        assert(replacementSnapshot.name == 'replacement-tool')
    )"),
              LUA_OK)
        << lua_tostring(m_lua, -1);

    // Invalidate an attached pad while retaining both devices in C++.
    const auto retainedParent = CTabletTool::create(nullptr);
    pad->m_parent             = retainedParent;
    ASSERT_EQ(luaL_dostring(m_lua, "assert(type(pad.belongs_to) == 'table')"), LUA_OK) << lua_tostring(m_lua, -1);
    pad->m_events.destroy.emit();
    ASSERT_EQ(luaL_dostring(m_lua, "assertExpired(pad)"), LUA_OK) << lua_tostring(m_lua, -1);
    pad.reset();
    ASSERT_EQ(luaL_dostring(m_lua, "assertExpired(pad)"), LUA_OK) << lua_tostring(m_lua, -1);
}

TEST_F(CConfigLuaDevice, switchUsesSuppliedAddressAndWeakIdentity) {
    auto first  = makeShared<CLuaDeviceSwitch>();
    auto second = makeShared<CLuaDeviceSwitch>();
    // An opaque address must never be dereferenced, nor used as device identity.
    Objects::CLuaDevice::push(m_lua, first, 0x1234);
    lua_setglobal(m_lua, "device");
    Objects::CLuaDevice::push(m_lua, first, 0x5678);
    lua_setglobal(m_lua, "alias");
    Objects::CLuaDevice::push(m_lua, second, 0x1234);
    lua_setglobal(m_lua, "other");
    ASSERT_EQ(luaL_dostring(m_lua, R"(
        assert(device.type == 'switch' and device.name == 'test-switch')
        assert(device.address == '0x1234')
        assert(device == alias and device ~= other)
        assert(device.default_speed == nil and device.active_keymap == nil)
    )"),
              LUA_OK)
        << lua_tostring(m_lua, -1);
    first->m_name = "renamed-switch";
    ASSERT_EQ(luaL_dostring(m_lua, "assert(device.name == 'renamed-switch')"), LUA_OK) << lua_tostring(m_lua, -1);
    first->events.destroy.emit();
    ASSERT_EQ(luaL_dostring(m_lua, "assertExpired(device); assertExpired(alias); assert(other.type == 'switch')"), LUA_OK) << lua_tostring(m_lua, -1);
    first.reset();
    second.reset();
    ASSERT_EQ(luaL_dostring(m_lua, "assertExpired(other); assert(device == alias and device ~= other)"), LUA_OK) << lua_tostring(m_lua, -1);
}

TEST_F(CConfigLuaDevice, collectionAndLuaCloseDisconnectDestructionListeners) {
    const auto mouse        = CMouse::create(nullptr);
    const auto switchDevice = makeShared<CLuaDeviceSwitch>();
    for (int i = 0; i < 32; ++i) {
        push(mouse);
        Objects::CLuaDevice::push(m_lua, switchDevice, 0x1234);
        lua_setglobal(m_lua, "switchDevice");
    }
    ASSERT_EQ(luaL_dostring(m_lua, R"(
        local weak = setmetatable({ device, switchDevice }, { __mode = 'v' })
        device = nil
        switchDevice = nil
        collectgarbage('collect')
        collectgarbage('collect')
        assert(next(weak) == nil)
    )"),
              LUA_OK)
        << lua_tostring(m_lua, -1);
    push(mouse);
    Objects::CLuaDevice::push(m_lua, switchDevice, 0x1234);
    lua_setglobal(m_lua, "switchDevice");
    mouse->m_events.destroy.emit();
    switchDevice->events.destroy.emit();
    ASSERT_EQ(luaL_dostring(m_lua, "assertExpired(device); assertExpired(switchDevice)"), LUA_OK) << lua_tostring(m_lua, -1);

    const auto retained       = CMouse::create(nullptr);
    const auto retainedSwitch = makeShared<CLuaDeviceSwitch>();
    push(retained);
    Objects::CLuaDevice::push(m_lua, retainedSwitch, 0x5678);
    lua_setglobal(m_lua, "retainedSwitch");
    lua_close(m_lua);
    m_lua = nullptr;
    retained->m_events.destroy.emit();
    retainedSwitch->events.destroy.emit();
}

TEST_F(CConfigLuaDevice, queryWithoutInputManagerReturnsEmptyArray) {
    auto              previous = std::move(g_pInputManager);
    const CScopeGuard GUARD    = {[&] { g_pInputManager = std::move(previous); }};
    lua_newtable(m_lua);
    Bindings::Internal::registerQueryBindings(m_lua);
    lua_setglobal(m_lua, "hl");
    ASSERT_EQ(luaL_dostring(m_lua, R"(
        local devices = hl.get_devices()
        assert(type(devices) == 'table' and #devices == 0 and next(devices) == nil)
    )"),
              LUA_OK)
        << lua_tostring(m_lua, -1);
}
