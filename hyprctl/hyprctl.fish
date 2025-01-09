function _hyprctl_2
    set 1 $argv[1]
    hyprctl monitors | awk '/Monitor/{ print $2 }'
end

function _hyprctl_4
    set 1 $argv[1]
    hyprctl clients | awk '/class/{print $2}'
end

function _hyprctl_3
    set 1 $argv[1]
    hyprctl devices | sed -n '/Keyboard at/{n; s/^\s\+//; p}'
end

function _hyprctl_1
    set 1 $argv[1]
    hyprpm list | awk '/Plugin/{print $4}'
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

    set literals "resizeactive" "2" "changegroupactive" "-r" "moveintogroup" "forceallowsinput" "4" "::=" "systeminfo" "all" "layouts" "setprop" "animationstyle" "switchxkblayout" "create" "denywindowfromgroup" "headless" "activebordercolor" "exec" "setcursor" "wayland" "focusurgentorlast" "workspacerules" "movecurrentworkspacetomonitor" "movetoworkspacesilent" "hyprpaper" "alpha" "inactivebordercolor" "movegroupwindow" "movecursortocorner" "movewindowpixel" "prev" "movewindow" "globalshortcuts" "clients" "dimaround" "setignoregrouplock" "splash" "execr" "monitors" "0" "forcenoborder" "-q" "animations" "1" "nomaxsize" "splitratio" "moveactive" "pass" "swapnext" "devices" "layers" "rounding" "lockactivegroup" "5" "moveworkspacetomonitor" "-f" "-i" "--quiet" "forcenodim" "pin" "0" "1" "forceopaque" "forcenoshadow" "setfloating" "minsize" "alphaoverride" "sendshortcut" "workspaces" "cyclenext" "alterzorder" "togglegroup" "lockgroups" "bordersize" "dpms" "focuscurrentorlast" "-1" "--batch" "notify" "remove" "instances" "1" "3" "moveoutofgroup" "killactive" "2" "movetoworkspace" "movecursor" "configerrors" "closewindow" "swapwindow" "tagwindow" "forcerendererreload" "centerwindow" "auto" "focuswindow" "seterror" "nofocus" "alphafullscreen" "binds" "version" "-h" "togglespecialworkspace" "fullscreen" "windowdancecompat" "0" "keyword" "toggleopaque" "3" "--instance" "togglefloating" "renameworkspace" "alphafullscreenoverride" "activeworkspace" "x11" "kill" "forceopaqueoverriden" "output" "global" "dispatch" "reload" "forcenoblur" "-j" "event" "--help" "disable" "-1" "activewindow" "keepaspectratio" "dismissnotify" "focusmonitor" "movefocus" "plugin" "exit" "workspace" "fullscreenstate" "getoption" "alphainactiveoverride" "alphainactive" "decorations" "settiled" "config-only" "descriptions" "resizewindowpixel" "fakefullscreen" "rollinglog" "swapactiveworkspaces" "submap" "next" "movewindoworgroup" "cursorpos" "forcenoanims" "focusworkspaceoncurrentmonitor" "maxsize" "movecursortomonitor"

    set descriptions
    set descriptions[1] "Resize the active window"
    set descriptions[2] "Fullscreen"
    set descriptions[3] "Switch to the next window in a group"
    set descriptions[4] "Refresh state after issuing the command"
    set descriptions[5] "Move the active window into a group"
    set descriptions[7] "CONFUSED"
    set descriptions[9] "Print system info"
    set descriptions[11] "List all layouts available (including plugin ones)"
    set descriptions[12] "Set a property of a window"
    set descriptions[14] "Set the xkb layout index for a keyboard"
    set descriptions[16] "Prohibit the active window from becoming or being inserted into group"
    set descriptions[19] "Execute a shell command"
    set descriptions[20] "Set the cursor theme and reloads the cursor manager"
    set descriptions[22] "Focus the urgent window or the last window"
    set descriptions[23] "Get the list of defined workspace rules"
    set descriptions[24] "Move the active workspace to a monitor"
    set descriptions[25] "Move window doesnt switch to the workspace"
    set descriptions[26] "Interact with hyprpaper if present"
    set descriptions[29] "Swap the active window with the next or previous in a group"
    set descriptions[30] "Move the cursor to the corner of the active window"
    set descriptions[31] "Move a selected window"
    set descriptions[33] "Move the active window in a direction or to a monitor"
    set descriptions[34] "Lists all global shortcuts"
    set descriptions[35] "List all windows with their properties"
    set descriptions[37] "Temporarily enable or disable binds:ignore_group_lock"
    set descriptions[38] "Print the current random splash"
    set descriptions[39] "Execute a raw shell command"
    set descriptions[40] "List active outputs with their properties"
    set descriptions[43] "Disable output"
    set descriptions[44] "Gets the current config info about animations and beziers"
    set descriptions[47] "Change the split ratio"
    set descriptions[48] "Move the active window"
    set descriptions[49] "Pass the key to a specified window"
    set descriptions[50] "Swap the focused window with the next window"
    set descriptions[51] "List all connected keyboards and mice"
    set descriptions[52] "List the layers"
    set descriptions[54] "Lock the focused group"
    set descriptions[55] "OK"
    set descriptions[56] "Move a workspace to a monitor"
    set descriptions[58] "Specify the Hyprland instance"
    set descriptions[59] "Disable output"
    set descriptions[61] "Pin a window"
    set descriptions[62] "WARNING"
    set descriptions[63] "INFO"
    set descriptions[66] "Set the current window's floating state to true"
    set descriptions[69] "On shortcut X sends shortcut Y to a specified window"
    set descriptions[70] "List all workspaces with their properties"
    set descriptions[71] "Focus the next window on a workspace"
    set descriptions[72] "Modify the window stack order of the active or specified window"
    set descriptions[73] "Toggle the current active window into a group"
    set descriptions[74] "Lock the groups"
    set descriptions[76] "Set all monitors' DPMS status"
    set descriptions[77] "Switch focus from current to previously focused window"
    set descriptions[78] "No Icon"
    set descriptions[79] "Execute a batch of commands separated by ;"
    set descriptions[80] "Send a notification using the built-in Hyprland notification system"
    set descriptions[82] "List all running Hyprland instances and their info"
    set descriptions[83] "Maximize no fullscreen"
    set descriptions[84] "Maximize and fullscreen"
    set descriptions[85] "Move the active window out of a group"
    set descriptions[86] "Close the active window"
    set descriptions[87] "HINT"
    set descriptions[88] "Move the focused window to a workspace"
    set descriptions[89] "Move the cursor to a specified position"
    set descriptions[90] "List all current config parsing errors"
    set descriptions[91] "Close a specified window"
    set descriptions[92] "Swap the active window with another window"
    set descriptions[93] "Apply a tag to the window"
    set descriptions[94] "Force the renderer to reload all resources and outputs"
    set descriptions[95] "Center the active window"
    set descriptions[97] "Focus the first window matching"
    set descriptions[98] "Set the hyprctl error string"
    set descriptions[101] "List all registered binds"
    set descriptions[102] "Print the Hyprland version: flags, commit and branch of build"
    set descriptions[103] "Prints the help message"
    set descriptions[104] "Toggle a special workspace on/off"
    set descriptions[105] "Toggle the focused window's fullscreen state"
    set descriptions[107] "None"
    set descriptions[108] "Issue a keyword to call a config keyword dynamically"
    set descriptions[109] "Toggle the current window to always be opaque"
    set descriptions[110] "ERROR"
    set descriptions[111] "Specify the Hyprland instance"
    set descriptions[112] "Toggle the current window's floating state"
    set descriptions[113] "Rename a workspace"
    set descriptions[115] "Get the active workspace name and its properties"
    set descriptions[117] "Get into a kill mode, where you can kill an app by clicking on it"
    set descriptions[119] "Allows adding/removing fake outputs to a specific backend"
    set descriptions[120] "Execute a Global Shortcut using the GlobalShortcuts portal"
    set descriptions[121] "Issue a dispatch to call a keybind dispatcher with an arg"
    set descriptions[122] "Force reload the config"
    set descriptions[124] "Output in JSON format"
    set descriptions[125] "Emits a custom event to socket2"
    set descriptions[126] "Prints the help message"
    set descriptions[128] "Current"
    set descriptions[129] "Get the active window name and its properties"
    set descriptions[131] "Dismiss all or up to amount of notifications"
    set descriptions[132] "Focus a monitor"
    set descriptions[133] "Move the focus in a direction"
    set descriptions[134] "Interact with a plugin"
    set descriptions[135] "Exit the compositor with no questions asked"
    set descriptions[136] "Change the workspace"
    set descriptions[137] "Sets the focused window’s fullscreen mode and the one sent to the client"
    set descriptions[138] "Get the config option status (values)"
    set descriptions[141] "List all decorations and their info"
    set descriptions[142] "Set the current window's floating state to false"
    set descriptions[144] "Return a parsable JSON with all the config options, descriptions, value types and ranges"
    set descriptions[145] "Resize a selected window"
    set descriptions[146] "Toggle the focused window's internal fullscreen state"
    set descriptions[147] "Print tail of the log"
    set descriptions[148] "Swap the active workspaces between two monitors"
    set descriptions[149] "Change the current mapping group"
    set descriptions[151] "Behave as moveintogroup"
    set descriptions[152] "Get the current cursor pos in global layout coordinates"
    set descriptions[154] "Focus the requested workspace"
    set descriptions[155] "Move the cursor to a specified monitor"

    set literal_transitions
    set literal_transitions[1] "set inputs 121 44 126 82 4 52 51 129 90 59 9 11 12 131 14 98 102 103 134 101 138 23 20 141 26 144 108 147 70 34 35 79 115 38 152 117 122 124 40 43 80 119; set tos 15 3 22 3 22 3 3 3 3 22 3 3 4 5 6 7 3 22 8 3 3 3 3 9 3 3 10 11 3 3 3 22 3 3 3 3 14 22 12 22 16 13"
    set literal_transitions[2] "set inputs 82 52 51 129 9 90 11 12 131 14 98 102 134 101 23 20 138 141 26 144 108 147 70 34 35 115 38 152 117 40 119 122 121 80 44; set tos 3 3 3 3 3 3 3 4 5 6 7 3 8 3 3 3 3 9 3 3 10 11 3 3 3 3 3 3 3 12 13 14 15 16 3"
    set literal_transitions[4] "set inputs 140 64 65 46 106 28 27 53 6 67 68 130 114 13 75 100 36 153 99 60 118 42 18 139 155 123; set tos 3 17 17 17 17 3 3 5 17 3 17 17 17 3 5 3 17 17 17 17 17 17 3 17 3 17"
    set literal_transitions[7] "set inputs 127; set tos 3"
    set literal_transitions[11] "set inputs 57; set tos 3"
    set literal_transitions[12] "set inputs 10; set tos 3"
    set literal_transitions[13] "set inputs 15 81; set tos 20 23"
    set literal_transitions[14] "set inputs 143; set tos 3"
    set literal_transitions[15] "set inputs 1 85 3 86 5 88 89 91 92 93 94 95 97 16 19 104 22 105 24 25 29 30 31 109 112 33 113 37 39 120 125 47 48 49 50 54 56 132 133 135 136 61 137 142 66 145 146 69 148 71 72 73 74 149 76 77 151 154 155; set tos 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 21 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3"
    set literal_transitions[16] "set inputs 87 7 110 62 78 55 63; set tos 5 5 5 5 5 5 5"
    set literal_transitions[17] "set inputs 41 45; set tos 3 3"
    set literal_transitions[18] "set inputs 8; set tos 24"
    set literal_transitions[19] "set inputs 32 150; set tos 3 3"
    set literal_transitions[20] "set inputs 96 17 116 21; set tos 3 3 3 3"
    set literal_transitions[21] "set inputs 107 83 128 2 84; set tos 3 3 3 3 3"
    set literal_transitions[24] "set inputs 58 111; set tos 22 22"

    set match_anything_transitions_from 7 8 1 23 6 5 3 19 12 9 10 14 11 2
    set match_anything_transitions_to 18 3 2 3 19 3 18 3 18 3 3 18 18 2

    set state 1
    set word_index 2
    while test $word_index -lt $COMP_CWORD
        set -- word $COMP_WORDS[$word_index]

        if set --query literal_transitions[$state] && test -n $literal_transitions[$state]
            set --erase inputs
            set --erase tos
            eval $literal_transitions[$state]

            if contains -- $word $literals
                set literal_matched 0
                for literal_id in (seq 1 (count $literals))
                    if test $literals[$literal_id] = $word
                        set index (contains --index -- $literal_id $inputs)
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
            set index (contains --index -- $state $match_anything_transitions_from)
            set state $match_anything_transitions_to[$index]
            set word_index (math $word_index + 1)
            continue
        end

        return 1
    end

    if set --query literal_transitions[$state] && test -n $literal_transitions[$state]
        set --erase inputs
        set --erase tos
        eval $literal_transitions[$state]
        for literal_id in $inputs
            if test -n $descriptions[$literal_id]
                printf '%s\t%s\n' $literals[$literal_id] $descriptions[$literal_id]
            else
                printf '%s\n' $literals[$literal_id]
            end
        end
    end

    set command_states 8 23 9 6
    set command_ids 1 2 4 3
    if contains $state $command_states
        set index (contains --index $state $command_states)
        set function_id $command_ids[$index]
        set function_name _hyprctl_$function_id
        set --erase inputs
        set --erase tos
        $function_name "$COMP_WORDS[$COMP_CWORD]"
    end

    return 0
end

complete --command hyprctl --no-files --arguments "(_hyprctl)"
