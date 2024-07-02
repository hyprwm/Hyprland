function _hyprctl_3
    set 1 $argv[1]
    hyprctl monitors | awk '/Monitor/{ print $2 }'
end

function _hyprctl_4
    set 1 $argv[1]
    hyprpm list | awk '/Plugin/{ print $4 }'
end

function _hyprctl_1
    set 1 $argv[1]
    hyprctl clients | awk '/class/{ print $2 }'
end

function _hyprctl_2
    set 1 $argv[1]
    hyprctl devices | sed -n '/Keyboard at/{n; s/^\s\+//; p}'
end

function _hyprctl
    set COMP_LINE (commandline --cut-at-cursor)

    set COMP_WORDS
    echo $COMP_LINE | read --tokenize --array COMP_WORDS
    if string match --quiet --regex '.*\s$' $COMP_LINE
        set COMP_CWORD (math (count $COMP_WORDS) + 1)
    else
        set COMP_CWORD (count $COMP_WORDS)
    end

    set --local literals "cyclenext" "globalshortcuts" "cursorpos" "bordersize" "renameworkspace" "animationstyle" "focuswindow" "0" "auto" "swapnext" "forceallowsinput" "moveactive" "activebordercolor" "alphafullscreen" "wayland" "layers" "minsize" "monitors" "1" "kill" "settiled" "3" "focusmonitor" "swapwindow" "moveoutofgroup" "notify" "movecursor" "setcursor" "seterror" "movecurrentworkspacetomonitor" "4" "nomaxsize" "forcenoanims" "setprop" "-i" "-q" "togglefloating" "workspacerules" "movetoworkspace" "disable" "setignoregrouplock" "workspaces" "movegroupwindow" "closewindow" "0" "--instance" "binds" "movewindow" "splitratio" "alpha" "denywindowfromgroup" "workspace" "configerrors" "togglegroup" "getoption" "forceopaque" "keepaspectratio" "killactive" "pass" "decorations" "devices" "focuscurrentorlast" "submap" "global" "alphafullscreenoverride" "forcerendererreload" "movewindowpixel" "headless" "version" "dpms" "resizeactive" "moveintogroup" "5" "alphaoverride" "setfloating" "rollinglog" "::=" "rounding" "layouts" "moveworkspacetomonitor" "exec" "alphainactiveoverride" "alterzorder" "fakefullscreen" "nofocus" "keyword" "forcenoborder" "forcenodim" "--quiet" "pin" "output" "forcenoblur" "togglespecialworkspace" "fullscreen" "toggleopaque" "focusworkspaceoncurrentmonitor" "next" "changegroupactive" "-j" "instances" "execr" "exit" "clients" "all" "--batch" "dismissnotify" "inactivebordercolor" "switchxkblayout" "movetoworkspacesilent" "tagwindow" "movewindoworgroup" "-r" "movefocus" "focusurgentorlast" "remove" "activeworkspace" "dispatch" "create" "centerwindow" "2" "hyprpaper" "-1" "reload" "alphainactive" "systeminfo" "plugin" "dimaround" "activewindow" "swapactiveworkspaces" "splash" "sendshortcut" "maxsize" "lockactivegroup" "windowdancecompat" "forceopaqueoverriden" "lockgroups" "movecursortocorner" "x11" "prev" "1" "resizewindowpixel" "forcenoshadow"

    set --local descriptions
    set descriptions[1] "Focus the next window on a workspace"
    set descriptions[3] "Get the current cursor pos in global layout coordinates"
    set descriptions[5] "Rename a workspace"
    set descriptions[7] "Focus the first window matching"
    set descriptions[10] "Swap the focused window with the next window"
    set descriptions[12] "Move the active window"
    set descriptions[16] "List the layers"
    set descriptions[18] "List active outputs with their properties"
    set descriptions[20] "Get into a kill mode, where you can kill an app by clicking on it"
    set descriptions[21] "Set the current window's floating state to false"
    set descriptions[22] "ERROR"
    set descriptions[23] "Focus a monitor"
    set descriptions[24] "Swap the active window with another window"
    set descriptions[25] "Move the active window out of a group"
    set descriptions[26] "Send a notification using the built-in Hyprland notification system"
    set descriptions[27] "Move the cursor to a specified position"
    set descriptions[28] "Set the cursor theme and reloads the cursor manager"
    set descriptions[29] "Set the hyprctl error string"
    set descriptions[30] "Move the active workspace to a monitor"
    set descriptions[31] "CONFUSED"
    set descriptions[34] "Set a property of a window"
    set descriptions[35] "Specify the Hyprland instance"
    set descriptions[36] "Disable output"
    set descriptions[37] "Toggle the current window's floating state"
    set descriptions[38] "Get the list of defined workspace rules"
    set descriptions[39] "Move the focused window to a workspace"
    set descriptions[41] "Temporarily enable or disable binds:ignore_group_lock"
    set descriptions[42] "List all workspaces with their properties"
    set descriptions[43] "Swap the active window with the next or previous in a group"
    set descriptions[44] "Close a specified window"
    set descriptions[45] "WARNING"
    set descriptions[46] "Specify the Hyprland instance"
    set descriptions[47] "List all registered binds"
    set descriptions[48] "Move the active window in a direction or to a monitor"
    set descriptions[49] "Change the split ratio"
    set descriptions[51] "Prohibit the active window from becoming or being inserted into group"
    set descriptions[52] "Change the workspace"
    set descriptions[53] "List all current config parsing errors"
    set descriptions[54] "Toggle the current active window into a group"
    set descriptions[55] "Get the config option status (values)"
    set descriptions[58] "Close the active window"
    set descriptions[59] "Pass the key to a specified window"
    set descriptions[60] "List all decorations and their info"
    set descriptions[61] "List all connected keyboards and mice"
    set descriptions[62] "Switch focus from current to previously focused window"
    set descriptions[63] "Change the current mapping group"
    set descriptions[64] "Execute a Global Shortcut using the GlobalShortcuts portal"
    set descriptions[66] "Force the renderer to reload all resources and outputs"
    set descriptions[67] "Move a selected window"
    set descriptions[69] "Print the Hyprland version: flags, commit and branch of build"
    set descriptions[70] "Set all monitors' DPMS status"
    set descriptions[71] "Resize the active window"
    set descriptions[72] "Move the active window into a group"
    set descriptions[73] "OK"
    set descriptions[75] "Set the current window's floating state to true"
    set descriptions[76] "Print tail of the log"
    set descriptions[79] "List all layouts available (including plugin ones)"
    set descriptions[80] "Move a workspace to a monitor"
    set descriptions[81] "Execute a shell command"
    set descriptions[83] "Modify the window stack order of the active or specified window"
    set descriptions[84] "Toggle the focused window's internal fullscreen state"
    set descriptions[86] "Issue a keyword to call a config keyword dynamically"
    set descriptions[89] "Disable output"
    set descriptions[90] "Pin a window"
    set descriptions[91] "Allows adding/removing fake outputs to a specific backend"
    set descriptions[93] "Toggle a special workspace on/off"
    set descriptions[94] "Toggle the focused window's fullscreen state"
    set descriptions[95] "Toggle the current window to always be opaque"
    set descriptions[96] "Focus the requested workspace"
    set descriptions[98] "Switch to the next window in a group"
    set descriptions[99] "Output in JSON format"
    set descriptions[100] "List all running Hyprland instances and their info"
    set descriptions[101] "Execute a raw shell command"
    set descriptions[102] "Exit the compositor with no questions asked"
    set descriptions[103] "List all windows with their properties"
    set descriptions[105] "Execute a batch of commands separated by ;"
    set descriptions[106] "Dismiss all or up to amount of notifications"
    set descriptions[108] "Set the xkb layout index for a keyboard"
    set descriptions[109] "Move window doesnt switch to the workspace"
    set descriptions[110] "Apply a tag to the window"
    set descriptions[111] "Behave as moveintogroup"
    set descriptions[112] "Refresh state after issuing the command"
    set descriptions[113] "Move the focus in a direction"
    set descriptions[114] "Focus the urgent window or the last window"
    set descriptions[116] "Get the active workspace name and its properties"
    set descriptions[117] "Issue a dispatch to call a keybind dispatcher with an arg"
    set descriptions[119] "Center the active window"
    set descriptions[120] "HINT"
    set descriptions[121] "Interact with hyprpaper if present"
    set descriptions[122] "No Icon"
    set descriptions[123] "Force reload the config"
    set descriptions[125] "Print system info"
    set descriptions[126] "Interact with a plugin"
    set descriptions[128] "Get the active window name and its properties"
    set descriptions[129] "Swap the active workspaces between two monitors"
    set descriptions[130] "Print the current random splash"
    set descriptions[131] "On shortcut X sends shortcut Y to a specified window"
    set descriptions[133] "Lock the focused group"
    set descriptions[136] "Lock the groups"
    set descriptions[137] "Move the cursor to the corner of the active window"
    set descriptions[140] "INFO"
    set descriptions[141] "Resize a selected window"

    set --local literal_transitions
    set literal_transitions[1] "set inputs 106 76 34 36 2 3 79 108 38 112 42 47 116 86 117 53 89 55 91 121 123 125 16 60 61 18 126 20 128 130 26 69 99 100 28 29 103 105; set tos 2 3 4 5 3 3 3 6 3 5 3 3 3 7 9 3 5 3 10 3 3 3 3 11 3 12 13 3 3 3 14 3 5 3 3 15 3 5"
    set literal_transitions[4] "set inputs 74 14 33 56 57 92 107 124 78 17 127 4 6 65 132 134 82 135 85 32 50 13 87 11 88 142; set tos 18 3 18 18 18 18 3 3 2 3 18 2 3 18 3 18 18 18 18 18 3 3 18 18 18 18"
    set literal_transitions[8] "set inputs 106 76 34 2 3 79 108 38 42 47 116 86 117 53 55 91 121 123 125 16 60 61 18 126 20 128 130 26 69 100 28 29 103; set tos 2 3 4 3 3 3 6 3 3 3 3 7 9 3 3 10 3 3 3 3 11 3 12 13 3 3 3 14 3 3 3 15 3"
    set literal_transitions[9] "set inputs 102 131 133 1 75 37 109 110 39 111 5 80 41 81 114 7 43 44 83 84 48 49 10 51 52 54 12 113 90 119 58 93 59 94 95 62 63 129 96 64 21 98 23 24 66 67 136 137 25 27 70 101 71 141 30 72; set tos 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3"
    set literal_transitions[10] "set inputs 118 115; set tos 21 17"
    set literal_transitions[12] "set inputs 104; set tos 3"
    set literal_transitions[14] "set inputs 22 120 31 140 122 45 73; set tos 2 2 2 2 2 2 2"
    set literal_transitions[15] "set inputs 40; set tos 3"
    set literal_transitions[16] "set inputs 139 97; set tos 3 3"
    set literal_transitions[18] "set inputs 19 8; set tos 3 3"
    set literal_transitions[19] "set inputs 77; set tos 20"
    set literal_transitions[20] "set inputs 35 46; set tos 5 5"
    set literal_transitions[21] "set inputs 9 68 15 138; set tos 3 3 3 3"

    set --local match_anything_transitions_from 2 1 7 16 11 6 15 8 3 17 13 12
    set --local match_anything_transitions_to 3 8 3 3 3 16 19 8 19 3 3 19

    set --local state 1
    set --local word_index 2
    while test $word_index -lt $COMP_CWORD
        set --local -- word $COMP_WORDS[$word_index]

        if set --query literal_transitions[$state] && test -n $literal_transitions[$state]
            set --local --erase inputs
            set --local --erase tos
            eval $literal_transitions[$state]

            if contains -- $word $literals
                set --local literal_matched 0
                for literal_id in (seq 1 (count $literals))
                    if test $literals[$literal_id] = $word
                        set --local index (contains --index -- $literal_id $inputs)
                        set state $tos[$index]
                        set word_index (math $word_index + 1)
                        set literal_matched 1
                        break
                    end
                end
                if test $literal_matched -ne 0
                    continue
                end
            end
        end

        if set --query match_anything_transitions_from[$state] && test -n $match_anything_transitions_from[$state]
            set --local index (contains --index -- $state $match_anything_transitions_from)
            set state $match_anything_transitions_to[$index]
            set word_index (math $word_index + 1)
            continue
        end

        return 1
    end

    if set --query literal_transitions[$state] && test -n $literal_transitions[$state]
        set --local --erase inputs
        set --local --erase tos
        eval $literal_transitions[$state]
        for literal_id in $inputs
            if test -n $descriptions[$literal_id]
                printf '%s\t%s\n' $literals[$literal_id] $descriptions[$literal_id]
            else
                printf '%s\n' $literals[$literal_id]
            end
        end
    end

    set command_states 6 17 13 11
    set command_ids 2 3 4 1
    if contains $state $command_states
        set --local index (contains --index $state $command_states)
        set --local function_id $command_ids[$index]
        set --local function_name _hyprctl_$function_id
        set --local --erase inputs
        set --local --erase tos
        $function_name "$COMP_WORDS[$COMP_CWORD]"
    end

    return 0
end

complete --command hyprctl --no-files --arguments "(_hyprctl)"
