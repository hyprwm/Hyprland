function _hyprctl_3
    set 1 $argv[1]
    hyprctl monitors | grep Monitor | awk '{ print $2 }'
end

function _hyprctl_1
    set 1 $argv[1]
    hyprpm list | grep "Plugin" | awk '{print $4}'
end

function _hyprctl_2
    set 1 $argv[1]
    hyprctl devices | sed -n '/Keyboard at/{n; s/^\s\+//; p}'
end

function _hyprctl_4
    set 1 $argv[1]
    hyprctl clients | grep class | awk '{print $2}'
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

    set --local literals "resizeactive" "changegroupactive" "-r" "moveintogroup" "forceallowsinput" "4" "::=" "systeminfo" "all" "layouts" "animationstyle" "setprop" "switchxkblayout" "create" "denywindowfromgroup" "headless" "activebordercolor" "exec" "setcursor" "wayland" "focusurgentorlast" "workspacerules" "movecurrentworkspacetomonitor" "movetoworkspacesilent" "hyprpaper" "alpha" "inactivebordercolor" "movegroupwindow" "movecursortocorner" "movewindowpixel" "prev" "movewindow" "clients" "dimaround" "setignoregrouplock" "splash" "execr" "monitors" "0" "forcenoborder" "1" "nomaxsize" "splitratio" "moveactive" "pass" "swapnext" "devices" "layers" "rounding" "lockactivegroup" "5" "moveworkspacetomonitor" "-i" "forcenodim" "pin" "0" "1" "forceopaque" "forcenoshadow" "setfloating" "minsize" "alphaoverride" "workspaces" "cyclenext" "alterzorder" "togglegroup" "lockgroups" "bordersize" "dpms" "focuscurrentorlast" "-1" "--batch" "notify" "remove" "instances" "moveoutofgroup" "killactive" "2" "movetoworkspace" "movecursor" "configerrors" "closewindow" "swapwindow" "auto" "forcerendererreload" "centerwindow" "focuswindow" "seterror" "nofocus" "alphafullscreen" "binds" "version" "togglespecialworkspace" "fullscreen" "windowdancecompat" "globalshortcuts" "keyword" "toggleopaque" "3" "--instance" "togglefloating" "renameworkspace" "alphafullscreenoverride" "activeworkspace" "x11" "kill" "forceopaqueoverriden" "output" "global" "dispatch" "reload" "forcenoblur" "-j" "disable" "activewindow" "keepaspectratio" "dismissnotify" "focusmonitor" "movefocus" "plugin" "exit" "workspace" "getoption" "alphainactiveoverride" "alphainactive" "decorations" "settiled" "resizewindowpixel" "fakefullscreen" "rollinglog" "swapactiveworkspaces" "submap" "next" "movewindoworgroup" "cursorpos" "forcenoanims" "focusworkspaceoncurrentmonitor" "maxsize"

    set --local descriptions
    set descriptions[1] "Resize the active window"
    set descriptions[2] "Switch to the next window in a group"
    set descriptions[3] "Refresh state after issuing the command"
    set descriptions[4] "Move the active window into a group"
    set descriptions[6] "CONFUSED"
    set descriptions[8] "Print system info"
    set descriptions[10] "List all layouts available (including plugin ones)"
    set descriptions[12] "Set a property of a window"
    set descriptions[13] "Set the xkb layout index for a keyboard"
    set descriptions[15] "Prohibit the active window from becoming or being inserted into group"
    set descriptions[18] "Execute a shell command"
    set descriptions[19] "Set the cursor theme and reloads the cursor manager"
    set descriptions[21] "Focus the urgent window or the last window"
    set descriptions[22] "Get the list of defined workspace rules"
    set descriptions[23] "Move the active workspace to a monitor"
    set descriptions[24] "Move window doesnt switch to the workspace"
    set descriptions[25] "Interact with hyprpaper if present"
    set descriptions[28] "Swap the active window with the next or previous in a group"
    set descriptions[29] "Move the cursor to the corner of the active window"
    set descriptions[30] "Move a selected window"
    set descriptions[32] "Move the active window in a direction or to a monitor"
    set descriptions[33] "List all windows with their properties"
    set descriptions[35] "Temporarily enable or disable binds:ignore_group_lock"
    set descriptions[36] "Print the current random splash"
    set descriptions[37] "Execute a raw shell command"
    set descriptions[38] "List active outputs with their properties"
    set descriptions[43] "Change the split ratio"
    set descriptions[44] "Move the active window"
    set descriptions[45] "Pass the key to a specified window"
    set descriptions[46] "Swap the focused window with the next window"
    set descriptions[47] "List all connected keyboards and mice"
    set descriptions[48] "List the layers"
    set descriptions[50] "Lock the focused group"
    set descriptions[51] "OK"
    set descriptions[52] "Move a workspace to a monitor"
    set descriptions[53] "Specify the Hyprland instance"
    set descriptions[55] "Pin a window"
    set descriptions[56] "WARNING"
    set descriptions[57] "INFO"
    set descriptions[60] "Set the current window's floating state to true"
    set descriptions[63] "List all workspaces with their properties"
    set descriptions[64] "Focus the next window on a workspace"
    set descriptions[65] "Modify the window stack order of the active or specified window"
    set descriptions[66] "Toggle the current active window into a group"
    set descriptions[67] "Lock the groups"
    set descriptions[69] "Set all monitors' DPMS status"
    set descriptions[70] "Switch focus from current to previously focused window"
    set descriptions[71] "No Icon"
    set descriptions[72] "Execute a batch of commands separated by ;"
    set descriptions[73] "Send a notification using the built-in Hyprland notification system"
    set descriptions[75] "List all running Hyprland instances and their info"
    set descriptions[76] "Move the active window out of a group"
    set descriptions[77] "Close the active window"
    set descriptions[78] "HINT"
    set descriptions[79] "Move the focused window to a workspace"
    set descriptions[80] "Move the cursor to a specified position"
    set descriptions[81] "List all current config parsing errors"
    set descriptions[82] "Close a specified window"
    set descriptions[83] "Swap the active window with another window"
    set descriptions[85] "Force the renderer to reload all resources and outputs"
    set descriptions[86] "Center the active window"
    set descriptions[87] "Focus the first window matching"
    set descriptions[88] "Set the hyprctl error string"
    set descriptions[91] "List all registered binds"
    set descriptions[92] "Print the Hyprland version: flags, commit and branch of build"
    set descriptions[93] "Toggle a special workspace on/off"
    set descriptions[94] "Toggle the focused window's fullscreen state"
    set descriptions[97] "Issue a keyword to call a config keyword dynamically"
    set descriptions[98] "Toggle the current window to always be opaque"
    set descriptions[99] "ERROR"
    set descriptions[100] "Specify the Hyprland instance"
    set descriptions[101] "Toggle the current window's floating state"
    set descriptions[102] "Rename a workspace"
    set descriptions[104] "Get the active workspace name and its properties"
    set descriptions[106] "Get into a kill mode, where you can kill an app by clicking on it"
    set descriptions[108] "Allows adding/removing fake outputs to a specific backend"
    set descriptions[109] "Execute a Global Shortcut using the GlobalShortcuts portal"
    set descriptions[110] "Issue a dispatch to call a keybind dispatcher with an arg"
    set descriptions[111] "Force reload the config"
    set descriptions[113] "Output in JSON format"
    set descriptions[115] "Get the active window name and its properties"
    set descriptions[117] "Dismiss all or up to amount of notifications"
    set descriptions[118] "Focus a monitor"
    set descriptions[119] "Move the focus in a direction"
    set descriptions[120] "Interact with a plugin"
    set descriptions[121] "Exit the compositor with no questions asked"
    set descriptions[122] "Change the workspace"
    set descriptions[123] "Get the config option status (values)"
    set descriptions[126] "List all decorations and their info"
    set descriptions[127] "Set the current window's floating state to false"
    set descriptions[128] "Resize a selected window"
    set descriptions[129] "Toggle the focused window's internal fullscreen state"
    set descriptions[130] "Print tail of the log"
    set descriptions[131] "Swap the active workspaces between two monitors"
    set descriptions[132] "Change the current mapping group"
    set descriptions[134] "Behave as moveintogroup"
    set descriptions[135] "Get the current cursor pos in global layout coordinates"
    set descriptions[137] "Focus the requested workspace"

    set --local literal_transitions
    set literal_transitions[1] "set inputs 75 3 48 47 115 8 81 10 12 117 13 88 92 120 91 22 19 123 126 25 96 97 130 63 104 33 72 36 135 106 38 108 73 110 111 113; set tos 3 18 3 3 3 3 3 3 4 5 6 7 3 8 3 3 3 3 9 3 3 10 3 3 3 3 18 3 3 3 11 12 14 13 3 18"
    set literal_transitions[2] "set inputs 48 47 115 8 81 10 12 117 13 88 92 120 91 22 19 123 126 25 96 97 130 63 104 33 36 106 38 108 111 110 73 135 75; set tos 3 3 3 3 3 3 4 5 6 7 3 8 3 3 3 3 9 3 3 10 3 3 3 3 3 3 11 12 3 13 14 3 3"
    set literal_transitions[4] "set inputs 125 58 59 42 95 27 26 49 5 61 62 116 103 11 68 90 34 136 89 54 107 40 17 124 138 112; set tos 3 15 15 15 15 3 3 5 15 3 15 15 15 3 5 3 15 15 15 15 15 15 3 15 3 15"
    set literal_transitions[7] "set inputs 114; set tos 3"
    set literal_transitions[11] "set inputs 9; set tos 3"
    set literal_transitions[12] "set inputs 14 74; set tos 20 19"
    set literal_transitions[13] "set inputs 1 76 2 77 43 44 4 45 46 79 80 50 82 52 83 85 86 118 87 119 15 121 55 122 18 93 21 94 23 127 24 60 128 28 29 30 98 129 131 64 32 65 66 67 101 69 35 70 37 102 132 109 134 137; set tos 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3"
    set literal_transitions[14] "set inputs 78 6 99 56 71 51 57; set tos 5 5 5 5 5 5 5"
    set literal_transitions[15] "set inputs 39 41; set tos 3 3"
    set literal_transitions[16] "set inputs 7; set tos 17"
    set literal_transitions[17] "set inputs 53 100; set tos 18 18"
    set literal_transitions[20] "set inputs 84 16 105 20; set tos 3 3 3 3"
    set literal_transitions[21] "set inputs 31 133; set tos 3 3"

    set --local match_anything_transitions_from 7 8 1 6 5 21 3 19 9 10 11 2
    set --local match_anything_transitions_to 16 3 2 21 3 3 16 3 3 3 16 2

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

    set command_states 8 9 19 6
    set command_ids 1 4 3 2
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
