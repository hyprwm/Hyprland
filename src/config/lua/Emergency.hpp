#pragma once

namespace Config::Lua {
    constexpr const char* EMERGENCY_PCALL = R"#(
hl.bind("SUPER + Q", hl.dsp.exec_cmd [[
    for term in kitty alacritty foot wezterm gnome-terminal xterm; do
        if command -v "$term" >/dev/null; then
            exec "$term"
        fi
    done
]])

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
