#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>

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
