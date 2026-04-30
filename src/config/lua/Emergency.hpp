#pragma once

namespace Config::Lua {
    constexpr const char* EMERGENCY_PCALL = R"#(
local function shell_ok(cmd)
    local a, b, c = os.execute(cmd)
    if type(a) == "number" then
        return a == 0
    end
    return a == true and b == "exit" and c == 0
end

local function first_installed(candidates)
    for _, bin in ipairs(candidates) do
        if shell_ok(("command -v %q >/dev/null 2>&1"):format(bin)) then
            return bin
        end
    end
    return nil
end

local function launch_first_installed(candidates)
    local term = first_installed(candidates)
    if not term then
        return false
    end
    hl.dispatch(hl.dsp.exec_cmd(term))
    return true
end

hl.bind("SUPER + Q", function()
    launch_first_installed({ "kitty", "alacritty", "foot", "wezterm", "gnome-terminal", "xterm" })
end)

hl.bind("SUPER + R", hl.dsp.exec_cmd("hyprland-run"))
hl.bind("SUPER + M", hl.dsp.exit())

hl.window_rule({
    name  = "move-hyprland-run",
    match = { class = "hyprland-run" },

    move  = "20 monitor_h-120",
    float = true,
})

)#";
}