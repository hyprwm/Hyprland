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

    set --local literals "cyclenext" "globalshortcuts" "cursorpos" "bordersize" "renameworkspace" "animationstyle" "focuswindow" "0" "auto" "swapnext" "forceallowsinput" "moveactive" "activebordercolor" "wayland" "layers" "minsize" "monitors" "1" "3" "settiled" "kill" "focusmonitor" "swapwindow" "moveoutofgroup" "notify" "movecursor" "setcursor" "seterror" "4" "movecurrentworkspacetomonitor" "nomaxsize" "forcenoanims" "setprop" "-i" "togglefloating" "workspacerules" "movetoworkspace" "disable" "setignoregrouplock" "workspaces" "0" "closewindow" "movegroupwindow" "binds" "movewindow" "splitratio" "alpha" "denywindowfromgroup" "workspace" "configerrors" "togglegroup" "getoption" "forceopaque" "keepaspectratio" "--instance" "killactive" "pass" "decorations" "devices" "focuscurrentorlast" "submap" "global" "headless" "forcerendererreload" "movewindowpixel" "version" "dpms" "resizeactive" "moveintogroup" "5" "alphaoverride" "setfloating" "rollinglog" "::=" "rounding" "layouts" "moveworkspacetomonitor" "exec" "alphainactiveoverride" "alterzorder" "fakefullscreen" "nofocus" "keyword" "forcenoborder" "forcenodim" "pin" "output" "forcenoblur" "togglespecialworkspace" "fullscreen" "toggleopaque" "focusworkspaceoncurrentmonitor" "next" "changegroupactive" "-j" "instances" "execr" "exit" "clients" "all" "--batch" "dismissnotify" "inactivebordercolor" "switchxkblayout" "movetoworkspacesilent" "movewindoworgroup" "-r" "movefocus" "focusurgentorlast" "remove" "activeworkspace" "dispatch" "create" "centerwindow" "2" "hyprpaper" "-1" "reload" "alphainactive" "systeminfo" "plugin" "dimaround" "activewindow" "swapactiveworkspaces" "splash" "maxsize" "lockactivegroup" "windowdancecompat" "forceopaqueoverriden" "lockgroups" "movecursortocorner" "x11" "prev" "1" "resizewindowpixel" "forcenoshadow"

    set --local descriptions
    set descriptions[1] "Focus the next window on a workspace"
    set descriptions[3] "Get the current cursor pos in global layout coordinates"
    set descriptions[5] "Rename a workspace"
    set descriptions[7] "Focus the first window matching"
    set descriptions[10] "Swap the focused window with the next window"
    set descriptions[12] "Move the active window"
    set descriptions[15] "List the layers"
    set descriptions[17] "List active outputs with their properties"
    set descriptions[19] "ERROR"
    set descriptions[20] "Set the current window's floating state to false"
    set descriptions[21] "Get into a kill mode, where you can kill an app by clicking on it"
    set descriptions[22] "Focus a monitor"
    set descriptions[23] "Swap the active window with another window"
    set descriptions[24] "Move the active window out of a group"
    set descriptions[25] "Send a notification using the built-in Hyprland notification system"
    set descriptions[26] "Move the cursor to a specified position"
    set descriptions[27] "Set the cursor theme and reloads the cursor manager"
    set descriptions[28] "Set the hyprctl error string"
    set descriptions[29] "CONFUSED"
    set descriptions[30] "Move the active workspace to a monitor"
    set descriptions[33] "Set a property of a window"
    set descriptions[34] "Specify the Hyprland instance"
    set descriptions[35] "Toggle the current window's floating state"
    set descriptions[36] "Get the list of defined workspace rules"
    set descriptions[37] "Move the focused window to a workspace"
    set descriptions[39] "Temporarily enable or disable binds:ignore_group_lock"
    set descriptions[40] "List all workspaces with their properties"
    set descriptions[41] "WARNING"
    set descriptions[42] "Close a specified window"
    set descriptions[43] "Swap the active window with the next or previous in a group"
    set descriptions[44] "List all registered binds"
    set descriptions[45] "Move the active window in a direction or to a monitor"
    set descriptions[46] "Change the split ratio"
    set descriptions[48] "Prohibit the active window from becoming or being inserted into group"
    set descriptions[49] "Change the workspace"
    set descriptions[50] "List all current config parsing errors"
    set descriptions[51] "Toggle the current active window into a group"
    set descriptions[52] "Get the config option status (values)"
    set descriptions[55] "Specify the Hyprland instance"
    set descriptions[56] "Close the active window"
    set descriptions[57] "Pass the key to a specified window"
    set descriptions[58] "List all decorations and their info"
    set descriptions[59] "List all connected keyboards and mice"
    set descriptions[60] "Switch focus from current to previously focused window"
    set descriptions[61] "Change the current mapping group"
    set descriptions[62] "Execute a Global Shortcut using the GlobalShortcuts portal"
    set descriptions[64] "Force the renderer to reload all resources and outputs"
    set descriptions[65] "Move a selected window"
    set descriptions[66] "Print the Hyprland version: flags, commit and branch of build"
    set descriptions[67] "Set all monitors' DPMS status"
    set descriptions[68] "Resize the active window"
    set descriptions[69] "Move the active window into a group"
    set descriptions[70] "OK"
    set descriptions[72] "Set the current window's floating state to true"
    set descriptions[73] "Print tail of the log"
    set descriptions[76] "List all layouts available (including plugin ones)"
    set descriptions[77] "Move a workspace to a monitor"
    set descriptions[78] "Execute a shell command"
    set descriptions[80] "Modify the window stack order of the active or specified window"
    set descriptions[81] "Toggle the focused window's internal fullscreen state"
    set descriptions[83] "Issue a keyword to call a config keyword dynamically"
    set descriptions[86] "Pin a window"
    set descriptions[87] "Allows adding/removing fake outputs to a specific backend"
    set descriptions[89] "Toggle a special workspace on/off"
    set descriptions[90] "Toggle the focused window's fullscreen state"
    set descriptions[91] "Toggle the current window to always be opaque"
    set descriptions[92] "Focus the requested workspace"
    set descriptions[94] "Switch to the next window in a group"
    set descriptions[95] "Output in JSON format"
    set descriptions[96] "List all running Hyprland instances and their info"
    set descriptions[97] "Execute a raw shell command"
    set descriptions[98] "Exit the compositor with no questions asked"
    set descriptions[99] "List all windows with their properties"
    set descriptions[101] "Execute a batch of commands separated by ;"
    set descriptions[102] "Dismiss all or up to amount of notifications"
    set descriptions[104] "Set the xkb layout index for a keyboard"
    set descriptions[105] "Move window doesnt switch to the workspace"
    set descriptions[106] "Behave as moveintogroup"
    set descriptions[107] "Refresh state after issuing the command"
    set descriptions[108] "Move the focus in a direction"
    set descriptions[109] "Focus the urgent window or the last window"
    set descriptions[111] "Get the active workspace name and its properties"
    set descriptions[112] "Issue a dispatch to call a keybind dispatcher with an arg"
    set descriptions[114] "Center the active window"
    set descriptions[115] "HINT"
    set descriptions[116] "Interact with hyprpaper if present"
    set descriptions[117] "No Icon"
    set descriptions[118] "Force reload the config"
    set descriptions[120] "Print system info"
    set descriptions[121] "Interact with a plugin"
    set descriptions[123] "Get the active window name and its properties"
    set descriptions[124] "Swap the active workspaces between two monitors"
    set descriptions[125] "Print the current random splash"
    set descriptions[127] "Lock the focused group"
    set descriptions[130] "Lock the groups"
    set descriptions[131] "Move the cursor to the corner of the active window"
    set descriptions[134] "INFO"
    set descriptions[135] "Resize a selected window"

    set --local literal_transitions
    set literal_transitions[1] "set inputs 102 73 33 2 3 76 104 36 107 40 44 111 83 112 50 52 87 116 118 120 15 58 59 17 121 21 123 125 25 66 95 96 27 28 99 101; set tos 2 3 4 3 3 3 5 3 6 3 3 3 7 9 3 3 10 3 3 3 3 11 3 12 13 3 3 3 14 3 6 3 3 15 3 6"
    set literal_transitions[4] "set inputs 71 32 53 54 88 103 119 75 16 122 4 6 126 128 79 129 82 31 47 13 84 11 85 136; set tos 27 27 27 27 27 3 3 2 3 27 2 3 3 27 27 27 27 27 3 3 27 27 27 27"
    set literal_transitions[8] "set inputs 102 73 33 2 3 76 104 36 40 44 111 83 112 50 52 87 116 118 120 15 58 59 17 121 21 123 125 25 66 96 27 28 99; set tos 2 3 4 3 3 3 5 3 3 3 3 7 9 3 3 10 3 3 3 3 11 3 12 13 3 3 3 14 3 3 3 15 3"
    set literal_transitions[9] "set inputs 127 130 1 72 35 105 37 106 5 77 39 78 109 7 43 42 80 81 45 46 10 108 49 51 12 114 86 48 56 89 57 90 91 60 61 124 92 62 20 94 22 23 64 65 24 131 26 67 97 68 30 135 69 98; set tos 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3"
    set literal_transitions[10] "set inputs 113 110; set tos 18 21"
    set literal_transitions[11] "set inputs 19 115 29 134 70 117; set tos 3 3 3 3 3 3"
    set literal_transitions[12] "set inputs 100; set tos 3"
    set literal_transitions[15] "set inputs 38; set tos 3"
    set literal_transitions[16] "set inputs 74; set tos 17"
    set literal_transitions[18] "set inputs 9 63 14 132; set tos 3 3 3 3"
    set literal_transitions[19] "set inputs 74; set tos 20"
    set literal_transitions[23] "set inputs 74; set tos 24"
    set literal_transitions[24] "set inputs 41; set tos 3"
    set literal_transitions[25] "set inputs 34 55; set tos 6 6"
    set literal_transitions[26] "set inputs 74; set tos 25"
    set literal_transitions[27] "set inputs 18 8; set tos 3 3"
    set literal_transitions[28] "set inputs 133 93; set tos 3 3"
    set literal_transitions[30] "set inputs 74; set tos 33"

    set --local match_anything_transitions_from 2 28 11 31 15 8 3 29 17 13 32 1 20 21 7 33 14 12 22 5
    set --local match_anything_transitions_to 3 3 32 23 26 8 26 30 22 3 16 8 29 3 3 31 2 26 19 28

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

    set command_states 33 17 20 11
    set command_ids 4 2 3 1
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
