-- Hyprtester Lua config

hl.monitor({ output = "HEADLESS-1", mode = "1920x1080@60", position = "auto-right", scale = "1" })
hl.monitor({ output = "HEADLESS-2", mode = "1920x1080@60", position = "auto-right", scale = "1" })
hl.monitor({ output = "HEADLESS-3", mode = "1920x1080@60", position = "auto-right", scale = "1" })
hl.monitor({ output = "HEADLESS-4", mode = "1920x1080@60", position = "auto-right", scale = "1" })
hl.monitor({ output = "HEADLESS-5", mode = "1920x1080@60", position = "auto-right", scale = "1" })
hl.monitor({ output = "HEADLESS-6", mode = "1920x1080@60", position = "auto-right", scale = "1" })
hl.monitor({ output = "HEADLESS-PERSISTENT-TEST", mode = "1920x1080@60", position = "auto-right", scale = "1" })
hl.monitor({ output = "", disabled = true })

local terminal = "kitty"
local fileManager = "dolphin"
local menu = "wofi --show drun"

hl.on("hyprland.start", function()
    hl.dispatch(hl.dsp.exec_cmd("sleep 0"))
end)

hl.env("XCURSOR_SIZE", "24")
hl.env("HYPRCURSOR_SIZE", "24")

hl.config({
    general = {
        gaps_in = 5,
        gaps_out = 20,
        border_size = 2,
        snap = {
            enabled = true,
            window_gap = 8,
            monitor_gap = 10,
            respect_gaps = false,
            border_overlap = false,
        },
        col = {
            active_border = { colors = { "rgba(33ccffee)", "rgba(00ff99ee)" }, angle = 45 },
            inactive_border = "rgba(595959aa)",
        },
        resize_on_border = false,
        allow_tearing = false,
        layout = "dwindle",
    },
})

hl.config({
    decoration = {
        rounding = 10,
        rounding_power = 2,
        active_opacity = 1.0,
        inactive_opacity = 1.0,
        shadow = {
            enabled = true,
            range = 4,
            render_power = 3,
            color = "rgba(1a1a1aee)",
        },
        blur = {
            enabled = true,
            size = 3,
            passes = 1,
            vibrancy = 0.1696,
        },
    },
})

hl.config({
    animations = {
        enabled = false,
    },
})

hl.curve("easeOutQuint", { type = "bezier", points = { {0.23, 1}, {0.32, 1} } })
hl.curve("easeInOutCubic", { type = "bezier", points = { {0.65, 0.05}, {0.36, 1} } })
hl.curve("linear", { type = "bezier", points = { {0, 0}, {1, 1} } })
hl.curve("almostLinear", { type = "bezier", points = { {0.5, 0.5}, {0.75, 1.0} } })
hl.curve("quick", { type = "bezier", points = { {0.15, 0}, {0.1, 1} } })

hl.animation({ leaf = "global", enabled = true, speed = 10, bezier = "default" })
hl.animation({ leaf = "border", enabled = true, speed = 5.39, bezier = "easeOutQuint" })
hl.animation({ leaf = "windows", enabled = true, speed = 4.79, bezier = "easeOutQuint" })
hl.animation({ leaf = "windowsIn", enabled = true, speed = 4.1, bezier = "easeOutQuint", style = "popin 87%" })
hl.animation({ leaf = "windowsOut", enabled = true, speed = 1.49, bezier = "linear", style = "popin 87%" })
hl.animation({ leaf = "fadeIn", enabled = true, speed = 1.73, bezier = "almostLinear" })
hl.animation({ leaf = "fadeOut", enabled = true, speed = 1.46, bezier = "almostLinear" })
hl.animation({ leaf = "fade", enabled = true, speed = 3.03, bezier = "quick" })
hl.animation({ leaf = "layers", enabled = true, speed = 3.81, bezier = "easeOutQuint" })
hl.animation({ leaf = "layersIn", enabled = true, speed = 4, bezier = "easeOutQuint", style = "fade" })
hl.animation({ leaf = "layersOut", enabled = true, speed = 1.5, bezier = "linear", style = "fade" })
hl.animation({ leaf = "fadeLayersIn", enabled = true, speed = 1.79, bezier = "almostLinear" })
hl.animation({ leaf = "fadeLayersOut", enabled = true, speed = 1.39, bezier = "almostLinear" })
hl.animation({ leaf = "workspaces", enabled = true, speed = 1.94, bezier = "almostLinear", style = "fade" })
hl.animation({ leaf = "workspacesIn", enabled = true, speed = 1.21, bezier = "almostLinear", style = "fade" })
hl.animation({ leaf = "workspacesOut", enabled = true, speed = 1.94, bezier = "almostLinear", style = "fade" })

hl.device({ name = "test-mouse-1", enabled = true })
hl.device({ name = "test-keyboard-1", enabled = true, tags = "test-tag"})

hl.config({
    dwindle = {
        preserve_split = true,
        split_bias = 1,
    },
})

hl.config({
    master = {
        new_status = "master",
    },
})

hl.config({
    scrolling = {
        fullscreen_on_one_column = true,
        column_width = 0.5,
        focus_fit_method = 1,
        follow_focus = true,
        follow_min_visible = 1,
        explicit_column_widths = "0.25, 0.333, 0.5, 0.667, 0.75, 1.0",
        wrap_focus = true,
        wrap_swapcol = true,
    },
})

hl.config({
    misc = {
        force_default_wallpaper = -1,
        disable_hyprland_logo = false,
    },
})

hl.config({
    input = {
        kb_layout = "us",
        kb_variant = "",
        kb_model = "",
        kb_options = "",
        kb_rules = "",
        follow_mouse = 1,
        sensitivity = 0,
        touchpad = {
            natural_scroll = false,
        },
    },
})

hl.device({
    name = "epic-mouse-v1",
    sensitivity = -0.5,
})

hl.config({
    debug = {
        disable_logs = false,
    },
})

local mainMod = "SUPER"

hl.bind(mainMod .. " + Q", hl.dsp.exec_cmd(terminal))
hl.bind(mainMod .. " + C", hl.dsp.window.close())
hl.bind(mainMod .. " + M", hl.dsp.exit())
hl.bind(mainMod .. " + E", hl.dsp.exec_cmd(fileManager))
hl.bind(mainMod .. " + V", hl.dsp.window.float({ action = "toggle" }))
hl.bind(mainMod .. " + R", hl.dsp.exec_cmd(menu))
hl.bind(mainMod .. " + P", hl.dsp.window.pseudo())
hl.bind(mainMod .. " + J", hl.dsp.layout("togglesplit"))

hl.bind(mainMod .. " + left", hl.dsp.focus({ direction = "left" }))
hl.bind(mainMod .. " + right", hl.dsp.focus({ direction = "right" }))
hl.bind(mainMod .. " + up", hl.dsp.focus({ direction = "up" }))
hl.bind(mainMod .. " + down", hl.dsp.focus({ direction = "down" }))

for i = 1, 10 do
    local key = i % 10
    hl.bind(mainMod .. " + " .. key, hl.dsp.focus({ workspace = tostring(i) }))
    hl.bind(mainMod .. " + SHIFT + " .. key, hl.dsp.window.move({ workspace = tostring(i) }))
end

hl.bind(mainMod .. " + S", hl.dsp.workspace.toggle_special("magic"))
hl.bind(mainMod .. " + SHIFT + S", hl.dsp.window.move({ workspace = "special:magic" }))

hl.bind(mainMod .. " + mouse_down", hl.dsp.focus({ workspace = "e+1" }))
hl.bind(mainMod .. " + mouse_up", hl.dsp.focus({ workspace = "e-1" }))

hl.bind(mainMod .. " + mouse:272", hl.dsp.window.drag(), { mouse = true })
hl.bind(mainMod .. " + mouse:273", hl.dsp.window.resize(), { mouse = true })

hl.bind("XF86AudioRaiseVolume", hl.dsp.exec_cmd("wpctl set-volume -l 1 @DEFAULT_AUDIO_SINK@ 5%+"), { locked = true, repeating = true })
hl.bind("XF86AudioLowerVolume", hl.dsp.exec_cmd("wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%-"), { locked = true, repeating = true })
hl.bind("XF86AudioMute", hl.dsp.exec_cmd("wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle"), { locked = true, repeating = true })
hl.bind("XF86AudioMicMute", hl.dsp.exec_cmd("wpctl set-mute @DEFAULT_AUDIO_SOURCE@ toggle"), { locked = true, repeating = true })
hl.bind("XF86MonBrightnessUp", hl.dsp.exec_cmd("brightnessctl s 10%+"), { locked = true, repeating = true })
hl.bind("XF86MonBrightnessDown", hl.dsp.exec_cmd("brightnessctl s 10%-"), { locked = true, repeating = true })

hl.bind("XF86AudioNext", hl.dsp.exec_cmd("playerctl next"), { locked = true })
hl.bind("XF86AudioPause", hl.dsp.exec_cmd("playerctl play-pause"), { locked = true })
hl.bind("XF86AudioPlay", hl.dsp.exec_cmd("playerctl play-pause"), { locked = true })
hl.bind("XF86AudioPrev", hl.dsp.exec_cmd("playerctl previous"), { locked = true })

hl.bind(mainMod .. " + u", hl.dsp.submap("submap1"))

hl.define_submap("submap1", function()
    hl.bind("u", hl.dsp.submap("submap2"))
    hl.bind("i", hl.dsp.submap("submap3"))
    hl.bind("o", hl.dsp.exec_cmd(terminal))
    hl.bind("p", hl.dsp.submap("reset"))
end)

hl.define_submap("submap2", "submap1", function()
    hl.bind("o", hl.dsp.exec_cmd(terminal))
end)

hl.define_submap("submap3", "reset", function()
    hl.bind("o", hl.dsp.exec_cmd(terminal))
end)

hl.window_rule({
    name = "suppress-maximize-events",
    match = { class = ".*" },
    suppress_event = "maximize",
})

hl.window_rule({
    name = "fix-xwayland-drags",
    match = {
        class = "^$",
        title = "^$",
        xwayland = true,
        float = true,
        fullscreen = false,
        pin = false,
    },
    no_focus = true,
})

hl.workspace_rule({ workspace = "n[s:window] w[tv1]", gaps_out = { top = 0, right = 0, bottom = 0, left = 0 }, gaps_in = { top = 0, right = 0, bottom = 0, left = 0 } })
hl.workspace_rule({ workspace = "n[s:window] f[1]", gaps_out = { top = 0, right = 0, bottom = 0, left = 0 }, gaps_in = { top = 0, right = 0, bottom = 0, left = 0 } })

hl.window_rule({
    name = "smart-gaps-1",
    match = { float = false, workspace = "n[s:window] w[tv1]" },
    border_size = 0,
    rounding = 0,
})

hl.window_rule({
    name = "smart-gaps-2",
    match = { float = false, workspace = "n[s:window] f[1]" },
    border_size = 0,
    rounding = 0,
})

hl.window_rule({
    name = "wr-kitty-stuff",
    match = { class = "wr_kitty" },
    float = true,
    size = "200 200",
    pin = false,
})

hl.window_rule({
    name = "tagged-kitty-floats",
    match = { tag = "tag_kitty" },
    float = true,
})

hl.window_rule({
    name = "static-kitty-tag",
    match = { class = "tag_kitty" },
    tag = "+tag_kitty",
})

hl.gesture({
    fingers = 3,
    direction = "left",
    action = function()
        hl.dispatch(hl.dsp.exec_cmd("kitty"))
    end,
})

hl.gesture({ fingers = 3, direction = "right", action = "float" })
hl.gesture({ fingers = 3, direction = "up", action = "close" })
hl.gesture({ fingers = 3, direction = "down", action = "fullscreen" })

hl.gesture({ fingers = 3, direction = "down", mods = "ALT", action = "float" })
hl.gesture({ fingers = 3, direction = "horizontal", mods = "ALT", action = "workspace" })

hl.gesture({ fingers = 5, direction = "up", action = function() hl.dispatch(hl.dsp.send_shortcut({ mods = "", key = "e", window = "activewindow" })) end })
hl.gesture({ fingers = 5, direction = "down", action = function() hl.dispatch(hl.dsp.send_shortcut({ mods = "", key = "x", window = "activewindow" })) end })
hl.gesture({ fingers = 5, direction = "left", action = function() hl.dispatch(hl.dsp.send_shortcut({ mods = "", key = "i", window = "activewindow" })) end })
hl.gesture({ fingers = 5, direction = "right", action = function() hl.dispatch(hl.dsp.send_shortcut({ mods = "", key = "t", window = "activewindow" })) end })
hl.gesture({ fingers = 4, direction = "right", action = function() hl.dispatch(hl.dsp.send_shortcut({ mods = "", key = "return", window = "activewindow" })) end })
hl.gesture({ fingers = 4, direction = "left", action = function() hl.dispatch(hl.dsp.cursor.move_to_corner({ corner = 1, window = "activewindow" })) end })
hl.gesture({ fingers = 2, direction = "pinch", action = "cursorZoom", zoom_level = "1", mode = "live" })

hl.gesture({ fingers = 2, direction = "right", action = "float", disable_inhibit = true })

hl.layout.register("columns", {
    recalculate = function(ctx)
        local n = #ctx.targets
        if n == 0 then
            return
        end

        for i, target in ipairs(ctx.targets) do
            target:place(ctx:column(i, n))
        end
    end,
})

hl.layout.register("grid", {
    recalculate = function(ctx)
        local n = #ctx.targets
        if n == 0 then
            return
        end

        local cols = math.ceil(math.sqrt(n))

        for i, target in ipairs(ctx.targets) do
            target:place(ctx:grid_cell(i, cols))
        end
    end,
})

