#pragma once

#include <string_view>

const std::string_view USAGE = R"#(usage: hyprctl [flags] <command> [args...|--help]

commands:
    activewindow        → Gets the active window name and its properties
    activeworkspace     → Gets the active workspace and its properties
    animations          → Gets the current config'd info about animations
                          and beziers
    binds               → Lists all registered binds
    clients             → Lists all windows with their properties
    configerrors        → Lists all current config parsing errors
    cursorpos           → Gets the current cursor position in global layout
                          coordinates
    decorations <window_regex> → Lists all decorations and their info
    devices             → Lists all connected keyboards and mice
    dismissnotify [amount] → Dismisses all or up to AMOUNT notifications
    dispatch <dispatcher> [args] → Issue a dispatch to call a keybind
                          dispatcher with arguments
    getoption <option>  → Gets the config option status (values)
    globalshortcuts     → Lists all global shortcuts
    hyprpaper ...       → Issue a hyprpaper request
    instances           → Lists all running instances of Hyprland with
                          their info
    keyword <name> <value> → Issue a keyword to call a config keyword
                          dynamically
    kill                → Issue a kill to get into a kill mode, where you can
                          kill an app by clicking on it. You can exit it
                          with ESCAPE
    layers              → Lists all the surface layers
    layouts             → Lists all layouts available (including plugin'd ones)
    monitors            → Lists active outputs with their properties,
                          'monitors all' lists active and inactive outputs
    notify ...          → Sends a notification using the built-in Hyprland
                          notification system
    output ...          → Allows you to add and remove fake outputs to your
                          preferred backend
    plugin ...          → Issue a plugin request
    reload [config-only] → Issue a reload to force reload the config. Pass
                          'config-only' to disable monitor reload
    rollinglog          → Prints tail of the log. Also supports -f/--follow
                          option
    setcursor <theme> <size> → Sets the cursor theme and reloads the cursor
                          manager
    seterror <color> <message...> → Sets the hyprctl error string. Color has
                          the same format as in colors in config. Will reset
                          when Hyprland's config is reloaded
    setprop ...         → Sets a window property
    splash              → Get the current splash
    switchxkblayout ... → Sets the xkb layout index for a keyboard
    systeminfo          → Get system info
    version             → Prints the hyprland version, meaning flags, commit
                          and branch of build.
    workspacerules      → Lists all workspace rules
    workspaces          → Lists all workspaces with their properties

flags:
    -j                  → Output in JSON
    -r                  → Refresh state after issuing command (e.g. for
                          updating variables)
    --batch             → Execute a batch of commands, separated by ';'
    --instance (-i)     → use a specific instance. Can be either signature or
                          index in hyprctl instances (0, 1, etc)
    --quiet (-q)        → Disable the output of hyprctl

--help:
    Can be used to print command's arguments that did not fit into this page
    (three dots))#";

const std::string_view HYPRPAPER_HELP = R"#(usage: hyprctl [flags] hyprpaper <request>

requests:
    listactive      → Lists all active images
    listloaded      → Lists all loaded images
    preload <path>  → Preloads image
    unload <path>   → Unloads image. Pass 'all' as path to unload all images
    wallpaper       → Issue a wallpaper to call a config wallpaper dynamically

flags:
    See 'hyprctl --help')#";

const std::string_view NOTIFY_HELP = R"#(usage: hyprctl [flags] notify <icon> <time_ms> <color> <message...>

icon:
    Integer of value:
        0       → Warning
        1       → Info
        2       → Hint
        3       → Error
        4       → Confused
        5       → Ok
        6 or -1 → No icon

time_ms:
    Time to display notification in milliseconds

color:
    Notification color. Format is the same as for colors in hyprland.conf. Use
    0 for default color for icon

message:
    Notification message

flags:
    See 'hyprctl --help')#";

const std::string_view OUTPUT_HELP = R"#(usage: hyprctl [flags] output <create <backend> | remove <name>>

create <backend>:
    Creates new virtual output. Possible values for backend: wayland, x11,
    headless or auto.

remove <name>:
    Removes virtual output. Pass the output's name, as found in
    'hyprctl monitors'

flags:
    See 'hyprctl --help')#";

const std::string_view PLUGIN_HELP = R"#(usage: hyprctl [flags] plugin <request>

requests:
    load <path>     → Loads a plugin. Path must be absolute
    unload <path>   → Unloads a plugin. Path must be absolute
    list [-t]       → Lists all loaded plugins

flags:
    -t              → Terse output mode
    See 'hyprctl --help')#";

const std::string_view SETPROP_HELP = R"#(usage: hyprctl [flags] setprop <regex> <property> <value> [lock]

regex:
    Regular expression by which a window will be searched

property:
    See https://wiki.hyprland.org/Configuring/Using-hyprctl/#setprop for list
    of properties

value:
    Property value

lock:
    Optional argument. If lock is not added, will be unlocked. Locking means a
    dynamic windowrule cannot override this setting.

flags:
    See 'hyprctl --help')#";

const std::string_view SWITCHXKBLAYOUT_HELP = R"#(usage: [flags] switchxkblayout <device> <cmd>

device:
    You can find the device using 'hyprctl devices' command

cmd:
    'next' for next, 'prev' for previous, or ID for a specific one. IDs are
    assigned based on their order in config file (keyboard_layout),
    starting from 0

flags:
    See 'hyprctl --help')#";
