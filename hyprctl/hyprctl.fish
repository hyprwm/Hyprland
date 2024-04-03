function _hyprctl_4
    set 1 $argv[1]
    hyprctl monitors | grep Monitor | awk '{ print $2 }'
end

function _hyprctl_3
    set 1 $argv[1]
    hyprpm list | grep "Plugin" | awk '{print $4}'
end

function _hyprctl_1
    set 1 $argv[1]
    hyprctl devices | sed -n '/Keyboard at/{n; s/^\s\+//; p}'
end

function _hyprctl_2
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

    set --local literals "focusmonitor" "exit" "global" "forceallowsinput" "::=" "movecursortocorner" "movewindowpixel" "activeworkspace" "monitors" "movecurrentworkspacetomonitor" "togglespecialworkspace" "all" "animationstyle" "closewindow" "setprop" "clients" "denywindowfromgroup" "create" "moveoutofgroup" "headless" "activebordercolor" "rollinglog" "wayland" "movewindoworgroup" "setcursor" "fakefullscreen" "moveactive" "prev" "hyprpaper" "alpha" "inactivebordercolor" "-i" "--instance" "togglefloating" "settiled" "swapwindow" "dimaround" "setignoregrouplock" "layouts" "0" "forcenoborder" "notify" "binds" "focuswindow" "seterror" "1" "systeminfo" "exec" "cyclenext" "nomaxsize" "reload" "rounding" "layers" "setfloating" "5" "lockactivegroup" "movetoworkspace" "swapactiveworkspaces" "changegroupactive" "forcenodim" "0" "configerrors" "4" "forceopaque" "forcenoshadow" "workspaces" "1" "swapnext" "minsize" "alphaoverride" "toggleopaque" "decorations" "alterzorder" "bordersize" "-1" "focuscurrentorlast" "workspacerules" "splitratio" "remove" "renameworkspace" "movetoworkspacesilent" "killactive" "pass" "getoption" "switchxkblayout" "2" "auto" "pin" "version" "nofocus" "togglegroup" "workspace" "lockgroups" "-r" "movewindow" "cursorpos" "focusworkspaceoncurrentmonitor" "execr" "windowdancecompat" "globalshortcuts" "3" "keyword" "movefocus" "movecursor" "instances" "dpms" "x11" "moveintogroup" "resizewindowpixel" "kill" "moveworkspacetomonitor" "forceopaqueoverriden" "dispatch" "-j" "forcenoblur" "devices" "disable" "-b" "activewindow" "fullscreen" "keepaspectratio" "output" "plugin" "alphainactiveoverride" "alphainactive" "resizeactive" "centerwindow" "splash" "focusurgentorlast" "submap" "next" "movegroupwindow" "forcenoanims" "forcerendererreload" "maxsize" "dismissnotify"

    set --local descriptions
    set descriptions[1] "focuses a monitor"
    set descriptions[2] "exits the compositor with no questions asked"
    set descriptions[3] "Executes a Global Shortcut using the GlobalShortcuts portal"
    set descriptions[6] "moves the cursor to the corner of the active window"
    set descriptions[7] "moves a selected window	resizeparams"
    set descriptions[8] "Gets the active workspace name and its properties"
    set descriptions[9] "lists active outputs with their properties"
    set descriptions[10] "Moves the active workspace to a monitor"
    set descriptions[11] "toggles a special workspace on/off"
    set descriptions[14] "closes a specified window"
    set descriptions[15] "Sets a property of a window"
    set descriptions[16] "Lists all windows with their properties"
    set descriptions[17] "Prohibit the active window from becoming or being inserted into group"
    set descriptions[19] "Moves the active window out of a group"
    set descriptions[22] "Prints tail of the log"
    set descriptions[24] "Behaves as moveintogroup"
    set descriptions[25] "Sets the cursor theme and reloads the cursor manager"
    set descriptions[26] "toggles the focused window’s internal fullscreen state"
    set descriptions[27] "moves the active window	resizeparams"
    set descriptions[29] "Interact with hyprpaper if present"
    set descriptions[32] "Specify the Hyprland instalnce"
    set descriptions[33] "Specify the Hyprland instalnce"
    set descriptions[34] "toggles the current window’s floating state"
    set descriptions[35] "sets the current window’s floating state to false"
    set descriptions[36] "swaps the active window with another window"
    set descriptions[38] "Temporarily enable or disable binds:ignore_group_lock"
    set descriptions[39] "lists all layouts available (including plugin'd ones)"
    set descriptions[42] "Sends a notification using the built-in Hyprland notification system"
    set descriptions[43] "Lists all registered binds"
    set descriptions[44] "focuses the first window matching"
    set descriptions[45] "Sets the hyprctl error string"
    set descriptions[47] "Prints system info"
    set descriptions[48] "executes a shell command"
    set descriptions[49] "focuses the next window on a workspace"
    set descriptions[51] "Force reloads the config"
    set descriptions[53] "List the layers"
    set descriptions[54] "sets the current window’s floating state to true"
    set descriptions[55] "OK"
    set descriptions[56] "Lock the focused group"
    set descriptions[57] "moves the focused window to a workspace"
    set descriptions[58] "Swaps the active workspaces between two monitors"
    set descriptions[59] "switches to the next window in a group"
    set descriptions[61] "WARNING"
    set descriptions[62] "Lists all current config parsing errors"
    set descriptions[63] "CONFISED"
    set descriptions[66] "Lists all workspaces with their properties"
    set descriptions[67] "INFOROR"
    set descriptions[68] "swaps the focused window with the next window"
    set descriptions[71] "toggles the current window to always be opaque"
    set descriptions[72] "Lists all decorations and their info"
    set descriptions[73] "Modify the window stack order of the active or specified window"
    set descriptions[75] "No Icon"
    set descriptions[76] "Switch focus from current to previously focused window"
    set descriptions[77] "Gets the list of defined workspace rules"
    set descriptions[78] "changes the split ratio"
    set descriptions[80] "rename a workspace"
    set descriptions[81] "move window doesnt switch to the workspace"
    set descriptions[82] "closes the active window"
    set descriptions[83] "passes the key to a specified window"
    set descriptions[84] "Gets the config option status (values)"
    set descriptions[85] "Sets the xkb layout index for a keyboard"
    set descriptions[86] "HINT"
    set descriptions[88] "pins a window"
    set descriptions[89] "Prints the Hyprland version, meaning flags, commit and branch of build"
    set descriptions[91] "toggles the current active window into a group"
    set descriptions[92] "changes the workspace"
    set descriptions[93] "Locks the groups"
    set descriptions[94] "Refresh state befor issuing the command"
    set descriptions[95] "moves the active window in a direction or to a monitor"
    set descriptions[96] "Gets the current cursor pos in global layout coordinates"
    set descriptions[97] "Focuses the requested workspace"
    set descriptions[98] "executes a raw shell command"
    set descriptions[101] "ERROR"
    set descriptions[102] "Issue a keyword to call a config keyword dynamically"
    set descriptions[103] "moves the focus in a direction"
    set descriptions[104] "moves the cursor to a specified position"
    set descriptions[105] "Lists all running instances of Hyprland with thir info"
    set descriptions[106] "sets all monitors’ DPMS status"
    set descriptions[108] "Moves the active window into a group"
    set descriptions[109] "resizes a selected window"
    set descriptions[110] "Get into a kill mode, where you can kill an app by clicking on it"
    set descriptions[111] "Moves a workspace to a monitor"
    set descriptions[113] "Issue a dispatch to call a keybind dispatcher with an arg"
    set descriptions[114] "Output in JSON format"
    set descriptions[116] "Lists all connected keyboards and mice"
    set descriptions[118] "Execute a batch of commands separated by ;"
    set descriptions[119] "Gets the active window name and its properties"
    set descriptions[120] "toggles the focused window’s fullscreen state"
    set descriptions[122] "Allows you to add and remove fake outputs to your preferred backend"
    set descriptions[123] "Interact with a plugin"
    set descriptions[126] "resizes the active window"
    set descriptions[127] "center the active window"
    set descriptions[128] "Prints the current random splash"
    set descriptions[129] "Focuses the urgent window or the last window"
    set descriptions[130] "Change the current mapping group"
    set descriptions[132] "Swaps the active window with the next or previous in a group"
    set descriptions[134] "forces the renderer to reload all resources and outputs"
    set descriptions[136] "Dismisses all or up to amount of notifications"

    set --local literal_transitions
    set literal_transitions[1] "set inputs 45 116 47 85 84 118 51 53 119 8 9 122 15 89 16 123 62 94 22 25 66 96 29 100 128 102 72 105 39 110 77 114 113 42 43 136; set tos 32 4 4 22 4 12 4 4 4 4 29 26 8 4 4 25 4 12 4 4 4 4 4 4 4 31 19 4 4 4 4 12 28 2 4 3"
    set literal_transitions[7] "set inputs 87 20 107 23; set tos 4 4 4 4"
    set literal_transitions[8] "set inputs 125 64 65 50 99 31 30 52 4 69 70 121 13 74 37 133 90 60 112 41 21 124 135 115; set tos 4 9 9 9 9 4 4 3 9 4 9 9 4 3 9 9 9 9 9 9 4 9 4 9"
    set literal_transitions[9] "set inputs 40 46; set tos 4 4"
    set literal_transitions[10] "set inputs 5; set tos 11"
    set literal_transitions[11] "set inputs 33 32; set tos 12 12"
    set literal_transitions[13] "set inputs 28 131; set tos 4 4"
    set literal_transitions[16] "set inputs 5; set tos 5"
    set literal_transitions[17] "set inputs 5; set tos 18"
    set literal_transitions[18] "set inputs 61; set tos 4"
    set literal_transitions[19] "set inputs 63 86 101 67 75 55; set tos 4 4 4 4 4 4"
    set literal_transitions[21] "set inputs 5; set tos 14"
    set literal_transitions[26] "set inputs 18 79; set tos 7 27"
    set literal_transitions[28] "set inputs 1 2 48 49 3 81 82 83 6 7 54 56 11 10 57 120 14 88 17 58 19 59 91 95 97 92 24 93 26 27 68 98 126 127 34 71 103 35 104 73 36 129 130 76 38 106 108 109 78 111 132 44 134 80; set tos 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4"
    set literal_transitions[29] "set inputs 12; set tos 4"
    set literal_transitions[30] "set inputs 43 116 45 51 84 85 53 119 8 9 122 15 89 16 123 62 22 25 66 96 29 100 128 102 72 105 39 110 77 113 42 47 136; set tos 4 4 32 4 4 22 4 4 4 29 26 8 4 4 25 4 4 4 4 4 4 4 4 31 19 4 4 4 4 28 2 4 3"
    set literal_transitions[32] "set inputs 117; set tos 4"
    set literal_transitions[33] "set inputs 5; set tos 23"

    set --local match_anything_transitions_from 1 5 24 3 19 29 32 25 30 14 15 2 23 20 6 13 31 4 22 27
    set --local match_anything_transitions_to 30 6 16 4 20 10 10 4 30 15 33 3 24 21 17 4 4 10 13 4

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

    set command_states 5 14 23 19
    set command_ids 1 3 4 2
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
