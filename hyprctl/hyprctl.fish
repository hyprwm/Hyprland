function _hyprctl_3
    set 1 $argv[1]
    hyprctl monitors | grep Monitor | awk '{ print $2 }'
end

function _hyprctl_2
    set 1 $argv[1]
    hyprpm list | grep "Plugin" | awk '{print $4}'
end

function _hyprctl_1
    set 1 $argv[1]
    hyprctl clients | grep class | awk '{print $2}'
end

function _hyprctl_4
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

    set --local literals "cyclenext" "globalshortcuts" "cursorpos" "bordersize" "renameworkspace" "animationstyle" "focuswindow" "0" "auto" "swapnext" "forceallowsinput" "moveactive" "activebordercolor" "alphafullscreen" "wayland" "layers" "minsize" "monitors" "1" "kill" "settiled" "3" "focusmonitor" "swapwindow" "moveoutofgroup" "notify" "movecursor" "setcursor" "seterror" "movecurrentworkspacetomonitor" "4" "nomaxsize" "forcenoanims" "setprop" "-i" "togglefloating" "workspacerules" "movetoworkspace" "disable" "setignoregrouplock" "workspaces" "movegroupwindow" "closewindow" "0" "--instance" "binds" "movewindow" "splitratio" "alpha" "denywindowfromgroup" "workspace" "configerrors" "togglegroup" "getoption" "forceopaque" "keepaspectratio" "killactive" "pass" "decorations" "devices" "focuscurrentorlast" "submap" "global" "alphafullscreenoverride" "forcerendererreload" "movewindowpixel" "headless" "version" "dpms" "resizeactive" "moveintogroup" "5" "alphaoverride" "setfloating" "rollinglog" "::=" "rounding" "layouts" "moveworkspacetomonitor" "exec" "alphainactiveoverride" "alterzorder" "fakefullscreen" "nofocus" "keyword" "forcenoborder" "forcenodim" "pin" "output" "forcenoblur" "togglespecialworkspace" "fullscreen" "toggleopaque" "focusworkspaceoncurrentmonitor" "next" "changegroupactive" "-j" "instances" "execr" "exit" "clients" "all" "--batch" "dismissnotify" "inactivebordercolor" "switchxkblayout" "movetoworkspacesilent" "movewindoworgroup" "-r" "movefocus" "focusurgentorlast" "remove" "activeworkspace" "dispatch" "create" "centerwindow" "2" "hyprpaper" "-1" "reload" "alphainactive" "systeminfo" "plugin" "dimaround" "activewindow" "swapactiveworkspaces" "splash" "maxsize" "lockactivegroup" "windowdancecompat" "forceopaqueoverriden" "lockgroups" "movecursortocorner" "x11" "prev" "1" "resizewindowpixel" "forcenoshadow"

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
    set descriptions[36] "Toggle the current window's floating state"
    set descriptions[37] "Get the list of defined workspace rules"
    set descriptions[38] "Move the focused window to a workspace"
    set descriptions[40] "Temporarily enable or disable binds:ignore_group_lock"
    set descriptions[41] "List all workspaces with their properties"
    set descriptions[42] "Swap the active window with the next or previous in a group"
    set descriptions[43] "Close a specified window"
    set descriptions[44] "WARNING"
    set descriptions[45] "Specify the Hyprland instance"
    set descriptions[46] "List all registered binds"
    set descriptions[47] "Move the active window in a direction or to a monitor"
    set descriptions[48] "Change the split ratio"
    set descriptions[50] "Prohibit the active window from becoming or being inserted into group"
    set descriptions[51] "Change the workspace"
    set descriptions[52] "List all current config parsing errors"
    set descriptions[53] "Toggle the current active window into a group"
    set descriptions[54] "Get the config option status (values)"
    set descriptions[57] "Close the active window"
    set descriptions[58] "Pass the key to a specified window"
    set descriptions[59] "List all decorations and their info"
    set descriptions[60] "List all connected keyboards and mice"
    set descriptions[61] "Switch focus from current to previously focused window"
    set descriptions[62] "Change the current mapping group"
    set descriptions[63] "Execute a Global Shortcut using the GlobalShortcuts portal"
    set descriptions[65] "Force the renderer to reload all resources and outputs"
    set descriptions[66] "Move a selected window"
    set descriptions[68] "Print the Hyprland version: flags, commit and branch of build"
    set descriptions[69] "Set all monitors' DPMS status"
    set descriptions[70] "Resize the active window"
    set descriptions[71] "Move the active window into a group"
    set descriptions[72] "OK"
    set descriptions[74] "Set the current window's floating state to true"
    set descriptions[75] "Print tail of the log"
    set descriptions[78] "List all layouts available (including plugin ones)"
    set descriptions[79] "Move a workspace to a monitor"
    set descriptions[80] "Execute a shell command"
    set descriptions[82] "Modify the window stack order of the active or specified window"
    set descriptions[83] "Toggle the focused window's internal fullscreen state"
    set descriptions[85] "Issue a keyword to call a config keyword dynamically"
    set descriptions[88] "Pin a window"
    set descriptions[89] "Allows adding/removing fake outputs to a specific backend"
    set descriptions[91] "Toggle a special workspace on/off"
    set descriptions[92] "Toggle the focused window's fullscreen state"
    set descriptions[93] "Toggle the current window to always be opaque"
    set descriptions[94] "Focus the requested workspace"
    set descriptions[96] "Switch to the next window in a group"
    set descriptions[97] "Output in JSON format"
    set descriptions[98] "List all running Hyprland instances and their info"
    set descriptions[99] "Execute a raw shell command"
    set descriptions[100] "Exit the compositor with no questions asked"
    set descriptions[101] "List all windows with their properties"
    set descriptions[103] "Execute a batch of commands separated by ;"
    set descriptions[104] "Dismiss all or up to amount of notifications"
    set descriptions[106] "Set the xkb layout index for a keyboard"
    set descriptions[107] "Move window doesnt switch to the workspace"
    set descriptions[108] "Behave as moveintogroup"
    set descriptions[109] "Refresh state after issuing the command"
    set descriptions[110] "Move the focus in a direction"
    set descriptions[111] "Focus the urgent window or the last window"
    set descriptions[113] "Get the active workspace name and its properties"
    set descriptions[114] "Issue a dispatch to call a keybind dispatcher with an arg"
    set descriptions[116] "Center the active window"
    set descriptions[117] "HINT"
    set descriptions[118] "Interact with hyprpaper if present"
    set descriptions[119] "No Icon"
    set descriptions[120] "Force reload the config"
    set descriptions[122] "Print system info"
    set descriptions[123] "Interact with a plugin"
    set descriptions[125] "Get the active window name and its properties"
    set descriptions[126] "Swap the active workspaces between two monitors"
    set descriptions[127] "Print the current random splash"
    set descriptions[129] "Lock the focused group"
    set descriptions[132] "Lock the groups"
    set descriptions[133] "Move the cursor to the corner of the active window"
    set descriptions[136] "INFO"
    set descriptions[137] "Resize a selected window"

    set --local literal_transitions
    set literal_transitions[1] "set inputs 104 75 34 2 3 78 106 37 109 41 46 113 85 114 52 54 89 118 120 122 16 59 60 18 123 20 125 127 26 68 97 98 28 29 101 103; set tos 2 3 4 3 3 3 5 3 6 3 3 3 7 9 3 3 10 3 3 3 3 11 3 12 13 3 3 3 14 3 6 3 3 15 3 6"
    set literal_transitions[4] "set inputs 73 14 33 55 56 90 105 121 77 17 124 4 6 64 128 130 81 131 84 32 49 13 86 11 87 138; set tos 19 3 19 19 19 19 3 3 2 3 19 2 3 19 3 19 19 19 19 19 3 3 19 19 19 19"
    set literal_transitions[8] "set inputs 104 75 34 2 3 78 106 37 41 46 113 85 114 52 54 89 118 120 122 16 59 60 18 123 20 125 127 26 68 98 28 29 101; set tos 2 3 4 3 3 3 5 3 3 3 3 7 9 3 3 10 3 3 3 3 11 3 12 13 3 3 3 14 3 3 3 15 3"
    set literal_transitions[9] "set inputs 129 132 1 74 36 107 38 108 5 79 40 80 111 7 42 43 82 83 47 48 10 110 51 53 12 116 88 50 57 91 58 92 93 61 62 126 94 63 21 96 23 24 65 66 25 133 27 69 99 70 30 137 71 100; set tos 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3"
    set literal_transitions[10] "set inputs 115 112; set tos 16 17"
    set literal_transitions[12] "set inputs 102; set tos 3"
    set literal_transitions[14] "set inputs 22 117 31 136 119 44 72; set tos 2 2 2 2 2 2 2"
    set literal_transitions[15] "set inputs 39; set tos 3"
    set literal_transitions[16] "set inputs 9 67 15 134; set tos 3 3 3 3"
    set literal_transitions[18] "set inputs 76; set tos 20"
    set literal_transitions[19] "set inputs 19 8; set tos 3 3"
    set literal_transitions[20] "set inputs 35 45; set tos 6 6"
    set literal_transitions[21] "set inputs 135 95; set tos 3 3"

    set --local match_anything_transitions_from 2 1 7 21 11 3 8 13 15 17 5 12
    set --local match_anything_transitions_to 3 8 3 3 3 18 8 3 18 3 21 18

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

    set command_states 17 5 13 11
    set command_ids 3 4 2 1
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
