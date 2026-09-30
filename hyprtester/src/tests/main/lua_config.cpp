#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"

#include <array>
#include <filesystem>
#include <string_view>
#include <hyprutils/utils/ScopeGuard.hpp>
#include <hyprutils/os/File.hpp>
#include <hyprutils/os/Process.hpp>

using namespace Hyprutils::Utils;

TEST_CASE(luaRequire) {
    constexpr auto EXPECTED = "absolute:relative:a:b";

    EXPECT(getFromSocket("/repl return _G.hyprtester_lua_require_result"), EXPECTED);

    OK(getFromSocket("/reload"));
    EXPECT(getFromSocket("/repl return _G.hyprtester_lua_require_result"), EXPECTED);
}

TEST_CASE(luaSpecialWorkspaceDeactivationEventNil) {
    constexpr auto SPECIAL_WORKSPACE = "lua_special_active_nil";

    CScopeGuard    guard = {[&]() {
        if (getFromSocket("/monitors").contains(std::format("(special:{})", SPECIAL_WORKSPACE)))
            getFromSocket(std::format("/dispatch hl.dsp.workspace.toggle_special('{}')", SPECIAL_WORKSPACE));

        getFromSocket("/eval do _G.hyprtester_special_active_subscription = nil; _G.hyprtester_special_active_event_count = nil; "
                      "_G.hyprtester_special_active_workspace_type = nil; _G.hyprtester_special_active_monitor_type = nil end");
    }};

    OK(getFromSocket("/eval do _G.hyprtester_special_active_event_count = 0; _G.hyprtester_special_active_subscription = hl.on('workspace.special_active', function(workspace, "
                     "monitor) _G.hyprtester_special_active_event_count = _G.hyprtester_special_active_event_count + 1; _G.hyprtester_special_active_workspace_type = "
                     "type(workspace); _G.hyprtester_special_active_monitor_type = type(monitor) end) end"));

    OK(getFromSocket(std::format("/dispatch hl.dsp.workspace.toggle_special('{}')", SPECIAL_WORKSPACE)));
    ASSERT(getFromSocket("/repl _G.hyprtester_special_active_event_count"), "1");
    ASSERT(getFromSocket("/repl _G.hyprtester_special_active_workspace_type"), "userdata");
    ASSERT(getFromSocket("/repl _G.hyprtester_special_active_monitor_type"), "userdata");

    OK(getFromSocket(std::format("/dispatch hl.dsp.workspace.toggle_special('{}')", SPECIAL_WORKSPACE)));
    ASSERT(getFromSocket("/repl _G.hyprtester_special_active_event_count"), "2");
    ASSERT(getFromSocket("/repl _G.hyprtester_special_active_workspace_type"), "nil");
    ASSERT(getFromSocket("/repl _G.hyprtester_special_active_monitor_type"), "userdata");
}

TEST_CASE(luaDefaultSpecialWorkspaceName) {
    CScopeGuard guard = {[&]() {
        if (getFromSocket("/monitors").contains("(special:special)"))
            getFromSocket("/eval hl.get_active_monitor():set_special_workspace(nil)");
    }};

    OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special()"));
    ASSERT_CONTAINS(getFromSocket("/monitors"), "(special:special)");
    OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special()"));
    ASSERT_NOT_CONTAINS(getFromSocket("/monitors"), "(special:special)");

    OK(getFromSocket("/eval hl.get_active_monitor():set_special_workspace('')"));
    ASSERT_CONTAINS(getFromSocket("/monitors"), "(special:special)");
    OK(getFromSocket("/eval hl.get_active_monitor():set_special_workspace(nil)"));
    ASSERT_NOT_CONTAINS(getFromSocket("/monitors"), "(special:special)");
}

TEST_CASE(luaEventConfigUnload) {
    std::error_code ec;
    std::filesystem::remove("/tmp/hyprtester-luaEventConfigUnload.txt", ec);
    OK(getFromSocket("/eval luaEventConfigUnload = 'luaEventConfigUnload'; hl.on('config.unload', function() os.execute('echo -n '..tostring(luaEventConfigUnload)..' > "
                     "/tmp/hyprtester-luaEventConfigUnload.txt') end)"));
    OK(getFromSocket("/reload"));
    EXPECT(Hyprutils::File::readFileAsString("/tmp/hyprtester-luaEventConfigUnload.txt").value_or("error"), "luaEventConfigUnload");
    std::filesystem::remove("/tmp/hyprtester-luaEventConfigUnload.txt", ec);
}

TEST_CASE(luaReloadConfig) {
    constexpr auto VAR = "normally_nonexistent_variable";

    OK(getFromSocket(std::format("/eval {} = true", VAR)));
    OK(getFromSocket("/dispatch hl.dsp.reload_config()"));
    EXPECT(getFromSocket(std::format("/repl return {}", VAR)), "nil");
}

TEST_CASE(luaDevicesEnumerationAndHyprctlParity) {
    // The test plugin supplies one pointer and two keyboards.
    OK(getFromSocket(R"(/eval do
        local devices = hl.get_devices()
        assert(type(devices) == 'table')
        local types = { pointer = true, keyboard = true, tablet = true, tablet_pad = true,
                        tablet_tool = true, touch = true, switch = true }
        local count, pointers, keyboards = 0, 0, 0
        local byAddress = {}
        for index, device in pairs(devices) do
            assert(type(index) == 'number' and index % 1 == 0 and index >= 1 and index <= #devices)
            assert(type(device) == 'userdata' and types[device.type])
            assert(type(device.address) == 'string' and device.address:match('^0x%x+$'))
            assert(not byAddress[device.address], 'duplicate device address')
            byAddress[device.address] = device
            count = count + 1
            if device.type == 'pointer' and device.name:match('^test%-mouse') then
                pointers = pointers + 1
                assert(device.default_speed == 0)
                assert(type(device.scroll_factor) == 'number')
            elseif device.type == 'keyboard' and device.name:match('^test%-keyboard') then
                keyboards = keyboards + 1
                assert(type(device.main) == 'boolean')
                assert(type(device.caps_lock) == 'boolean' and type(device.num_lock) == 'boolean')
            end
        end
        assert(count == #devices and pointers == 1 and keyboards == 2)
        local again = hl.get_devices()
        assert(#again == count)
        for _, device in ipairs(again) do
            assert(device == byAddress[device.address])
        end
    end)"));

    // Convert the independent IPC snapshot into Lua assertions, including field-name
    // translations. jq's JSON quoting also quotes these device strings for Lua.
    Hyprutils::OS::CProcess parity("bash", {"-c", R"(set -o pipefail
        hyprctl -j devices | jq -r '
            def lua_fields:
                to_entries | map("[" + (.key | tojson) + "]=" + (.value | tojson)) | join(",");
            ([.mice[] | . + {type: "pointer", default_speed: .defaultSpeed, scroll_factor: .scrollFactor}
                | del(.defaultSpeed, .scrollFactor)] +
             [.keyboards[] | . + {type: "keyboard", caps_lock: .capsLock, num_lock: .numLock}
                | del(.capsLock, .numLock)]) as $devices |
            "local expected = {" + ($devices | map("{" + lua_fields + "}") | join(",")) + "}; " +
            "local actual = hl.get_devices(); assert(#actual == " +
                ([.mice, .keyboards, .tablets, .touch, .switches] | map(length) | add | tostring) + "); " +
            "local byAddress = {}; for _, device in ipairs(actual) do byAddress[device.address] = device end; " +
            "for _, expectedDevice in ipairs(expected) do " +
                "local device = assert(byAddress[expectedDevice.address]); " +
                "for field, value in pairs(expectedDevice) do " +
                    "if field == \"default_speed\" or field == \"scroll_factor\" then " +
                        "assert(math.abs(device[field] - value) < 0.0051, field) " +
                    "else assert(device[field] == value, expectedDevice.name .. \": \" .. field) end " +
                "end " +
            "end"
        '
    )"});
    parity.addEnv("HYPRLAND_INSTANCE_SIGNATURE", HIS);
    parity.runSync();
    ASSERT(parity.exitCode(), 0);
    ASSERT(parity.stdOut().empty(), false);
    OK(getFromSocket("/eval " + parity.stdOut()));

    const auto READONLY_RESULT = getFromSocket(R"(/eval do
        local device = assert(hl.get_devices()[1])
        local original = device.address
        assert(pcall(function() device.address = 'changed' end))
        assert(device.address == original)
    end)");
    ASSERT_CONTAINS(READONLY_RESULT, "attempt to modify read-only hl object");
    ASSERT_NOT_CONTAINS(READONLY_RESULT, "assertion failed");
}

TEST_CASE(luaDevicesRetainedKeyboardTracksLayoutChanges) {
    CScopeGuard guard = {[&]() {
        getFromSocket("/eval _G.hyprtester_lua_keyboard = nil");
        getFromSocket("/reload");
    }};

    OK(getFromSocket("r/eval hl.config({ input = { kb_layout = 'us,pl,ua', kb_variant = '', kb_options = '' } })"));
    OK(getFromSocket(R"(/eval do
        for _, device in ipairs(hl.get_devices()) do
            if device.type == 'keyboard' and device.name:match('^test%-keyboard') then
                _G.hyprtester_lua_keyboard = device
                break
            end
        end
        assert(_G.hyprtester_lua_keyboard, 'synthetic keyboard missing')
        assert(_G.hyprtester_lua_keyboard.layout == 'us,pl,ua')
    end)"));

    const std::array<std::string_view, 3> LAYOUT_NAMES = {
        "English (US)",
        "Polish",
        "Ukrainian",
    };
    for (size_t i = 0; i < LAYOUT_NAMES.size(); ++i) {
        OK(getFromSocket(std::format("/switchxkblayout all {}", i)));
        EXPECT(getFromSocket("/repl return _G.hyprtester_lua_keyboard.active_layout_index"), std::to_string(i));
        EXPECT(getFromSocket("/repl return _G.hyprtester_lua_keyboard.active_keymap"), std::string{LAYOUT_NAMES[i]});
        OK(getFromSocket(R"(/eval do
            local retained = _G.hyprtester_lua_keyboard
            local found = false
            for _, device in ipairs(hl.get_devices()) do
                if device.address == retained.address then
                    assert(device == retained)
                    assert(device.active_layout_index == retained.active_layout_index)
                    assert(device.active_keymap == retained.active_keymap)
                    found = true
                end
            end
            assert(found)
        end)"));
    }

    // Rebuilding the keymap must also be visible through the original userdata.
    OK(getFromSocket("r/eval hl.config({ input = { kb_layout = 'de', kb_variant = 'nodeadkeys' } })"));
    OK(getFromSocket(R"(/eval do
        local keyboard = _G.hyprtester_lua_keyboard
        assert(keyboard.layout == 'de' and keyboard.variant == 'nodeadkeys')
        assert(keyboard.active_layout_index == 0)
        assert(type(keyboard.active_keymap) == 'string')
    end)"));
}
