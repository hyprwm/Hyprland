_hyprctl_cmd_2 () {
    hyprctl monitors | grep Monitor | awk '{ print $2 }'
}

_hyprctl_cmd_1 () {
    hyprpm list | grep "Plugin" | awk '{print $4}'
}

_hyprctl_cmd_0 () {
    hyprctl clients | grep class | awk '{print $2}'
}

_hyprctl_cmd_3 () {
    hyprctl devices | sed -n '/Keyboard at/{n; s/^\s\+//; p}'
}

_hyprctl () {
    local -a literals=("cyclenext" "globalshortcuts" "cursorpos" "bordersize" "renameworkspace" "animationstyle" "focuswindow" "0" "auto" "swapnext" "forceallowsinput" "moveactive" "activebordercolor" "wayland" "layers" "minsize" "monitors" "1" "3" "settiled" "kill" "focusmonitor" "swapwindow" "moveoutofgroup" "notify" "movecursor" "setcursor" "seterror" "4" "movecurrentworkspacetomonitor" "nomaxsize" "forcenoanims" "setprop" "-i" "togglefloating" "workspacerules" "movetoworkspace" "disable" "setignoregrouplock" "workspaces" "0" "closewindow" "movegroupwindow" "binds" "movewindow" "splitratio" "alpha" "denywindowfromgroup" "workspace" "configerrors" "togglegroup" "getoption" "forceopaque" "keepaspectratio" "--instance" "killactive" "pass" "decorations" "devices" "focuscurrentorlast" "submap" "global" "headless" "forcerendererreload" "movewindowpixel" "version" "dpms" "resizeactive" "moveintogroup" "5" "alphaoverride" "setfloating" "rollinglog" "::=" "rounding" "layouts" "moveworkspacetomonitor" "exec" "alphainactiveoverride" "alterzorder" "fakefullscreen" "nofocus" "keyword" "forcenoborder" "forcenodim" "pin" "output" "forcenoblur" "togglespecialworkspace" "fullscreen" "toggleopaque" "focusworkspaceoncurrentmonitor" "next" "changegroupactive" "-j" "instances" "execr" "exit" "clients" "all" "--batch" "dismissnotify" "inactivebordercolor" "switchxkblayout" "movetoworkspacesilent" "movewindoworgroup" "-r" "movefocus" "focusurgentorlast" "remove" "activeworkspace" "dispatch" "create" "centerwindow" "2" "hyprpaper" "-1" "reload" "alphainactive" "systeminfo" "plugin" "dimaround" "activewindow" "swapactiveworkspaces" "splash" "maxsize" "lockactivegroup" "windowdancecompat" "forceopaqueoverriden" "lockgroups" "movecursortocorner" "x11" "prev" "1" "resizewindowpixel" "forcenoshadow")

    local -A descriptions
    descriptions[1]="Focus the next window on a workspace"
    descriptions[3]="Get the current cursor pos in global layout coordinates"
    descriptions[5]="Rename a workspace"
    descriptions[7]="Focus the first window matching"
    descriptions[10]="Swap the focused window with the next window"
    descriptions[12]="Move the active window"
    descriptions[15]="List the layers"
    descriptions[17]="List active outputs with their properties"
    descriptions[19]="ERROR"
    descriptions[20]="Set the current window's floating state to false"
    descriptions[21]="Get into a kill mode, where you can kill an app by clicking on it"
    descriptions[22]="Focus a monitor"
    descriptions[23]="Swap the active window with another window"
    descriptions[24]="Move the active window out of a group"
    descriptions[25]="Send a notification using the built-in Hyprland notification system"
    descriptions[26]="Move the cursor to a specified position"
    descriptions[27]="Set the cursor theme and reloads the cursor manager"
    descriptions[28]="Set the hyprctl error string"
    descriptions[29]="CONFUSED"
    descriptions[30]="Move the active workspace to a monitor"
    descriptions[33]="Set a property of a window"
    descriptions[34]="Specify the Hyprland instance"
    descriptions[35]="Toggle the current window's floating state"
    descriptions[36]="Get the list of defined workspace rules"
    descriptions[37]="Move the focused window to a workspace"
    descriptions[39]="Temporarily enable or disable binds:ignore_group_lock"
    descriptions[40]="List all workspaces with their properties"
    descriptions[41]="WARNING"
    descriptions[42]="Close a specified window"
    descriptions[43]="Swap the active window with the next or previous in a group"
    descriptions[44]="List all registered binds"
    descriptions[45]="Move the active window in a direction or to a monitor"
    descriptions[46]="Change the split ratio"
    descriptions[48]="Prohibit the active window from becoming or being inserted into group"
    descriptions[49]="Change the workspace"
    descriptions[50]="List all current config parsing errors"
    descriptions[51]="Toggle the current active window into a group"
    descriptions[52]="Get the config option status (values)"
    descriptions[55]="Specify the Hyprland instance"
    descriptions[56]="Close the active window"
    descriptions[57]="Pass the key to a specified window"
    descriptions[58]="List all decorations and their info"
    descriptions[59]="List all connected keyboards and mice"
    descriptions[60]="Switch focus from current to previously focused window"
    descriptions[61]="Change the current mapping group"
    descriptions[62]="Execute a Global Shortcut using the GlobalShortcuts portal"
    descriptions[64]="Force the renderer to reload all resources and outputs"
    descriptions[65]="Move a selected window"
    descriptions[66]="Print the Hyprland version: flags, commit and branch of build"
    descriptions[67]="Set all monitors' DPMS status"
    descriptions[68]="Resize the active window"
    descriptions[69]="Move the active window into a group"
    descriptions[70]="OK"
    descriptions[72]="Set the current window's floating state to true"
    descriptions[73]="Print tail of the log"
    descriptions[76]="List all layouts available (including plugin ones)"
    descriptions[77]="Move a workspace to a monitor"
    descriptions[78]="Execute a shell command"
    descriptions[80]="Modify the window stack order of the active or specified window"
    descriptions[81]="Toggle the focused window's internal fullscreen state"
    descriptions[83]="Issue a keyword to call a config keyword dynamically"
    descriptions[86]="Pin a window"
    descriptions[87]="Allows adding/removing fake outputs to a specific backend"
    descriptions[89]="Toggle a special workspace on/off"
    descriptions[90]="Toggle the focused window's fullscreen state"
    descriptions[91]="Toggle the current window to always be opaque"
    descriptions[92]="Focus the requested workspace"
    descriptions[94]="Switch to the next window in a group"
    descriptions[95]="Output in JSON format"
    descriptions[96]="List all running Hyprland instances and their info"
    descriptions[97]="Execute a raw shell command"
    descriptions[98]="Exit the compositor with no questions asked"
    descriptions[99]="List all windows with their properties"
    descriptions[101]="Execute a batch of commands separated by ;"
    descriptions[102]="Dismiss all or up to amount of notifications"
    descriptions[104]="Set the xkb layout index for a keyboard"
    descriptions[105]="Move window doesnt switch to the workspace"
    descriptions[106]="Behave as moveintogroup"
    descriptions[107]="Refresh state after issuing the command"
    descriptions[108]="Move the focus in a direction"
    descriptions[109]="Focus the urgent window or the last window"
    descriptions[111]="Get the active workspace name and its properties"
    descriptions[112]="Issue a dispatch to call a keybind dispatcher with an arg"
    descriptions[114]="Center the active window"
    descriptions[115]="HINT"
    descriptions[116]="Interact with hyprpaper if present"
    descriptions[117]="No Icon"
    descriptions[118]="Force reload the config"
    descriptions[120]="Print system info"
    descriptions[121]="Interact with a plugin"
    descriptions[123]="Get the active window name and its properties"
    descriptions[124]="Swap the active workspaces between two monitors"
    descriptions[125]="Print the current random splash"
    descriptions[127]="Lock the focused group"
    descriptions[130]="Lock the groups"
    descriptions[131]="Move the cursor to the corner of the active window"
    descriptions[134]="INFO"
    descriptions[135]="Resize a selected window"

    local -A literal_transitions
    literal_transitions[1]="([102]=2 [73]=3 [33]=4 [2]=3 [3]=3 [76]=3 [104]=5 [36]=3 [107]=6 [40]=3 [44]=3 [111]=3 [83]=7 [112]=9 [50]=3 [52]=3 [87]=10 [116]=3 [118]=3 [120]=3 [15]=3 [58]=11 [59]=3 [17]=12 [121]=13 [21]=3 [123]=3 [125]=3 [25]=14 [66]=3 [95]=6 [96]=3 [27]=3 [28]=15 [99]=3 [101]=6)"
    literal_transitions[4]="([71]=27 [32]=27 [53]=27 [54]=27 [88]=27 [103]=3 [119]=3 [75]=2 [16]=3 [122]=27 [4]=2 [6]=3 [126]=3 [128]=27 [79]=27 [129]=27 [82]=27 [31]=27 [47]=3 [13]=3 [84]=27 [11]=27 [85]=27 [136]=27)"
    literal_transitions[8]="([102]=2 [73]=3 [33]=4 [2]=3 [3]=3 [76]=3 [104]=5 [36]=3 [40]=3 [44]=3 [111]=3 [83]=7 [112]=9 [50]=3 [52]=3 [87]=10 [116]=3 [118]=3 [120]=3 [15]=3 [58]=11 [59]=3 [17]=12 [121]=13 [21]=3 [123]=3 [125]=3 [25]=14 [66]=3 [96]=3 [27]=3 [28]=15 [99]=3)"
    literal_transitions[9]="([127]=3 [130]=3 [1]=3 [72]=3 [35]=3 [105]=3 [37]=3 [106]=3 [5]=3 [77]=3 [39]=3 [78]=3 [109]=3 [7]=3 [43]=3 [42]=3 [80]=3 [81]=3 [45]=3 [46]=3 [10]=3 [108]=3 [49]=3 [51]=3 [12]=3 [114]=3 [86]=3 [48]=3 [56]=3 [89]=3 [57]=3 [90]=3 [91]=3 [60]=3 [61]=3 [124]=3 [92]=3 [62]=3 [20]=3 [94]=3 [22]=3 [23]=3 [64]=3 [65]=3 [24]=3 [131]=3 [26]=3 [67]=3 [97]=3 [68]=3 [30]=3 [135]=3 [69]=3 [98]=3)"
    literal_transitions[10]="([113]=18 [110]=21)"
    literal_transitions[11]="([19]=3 [115]=3 [29]=3 [134]=3 [70]=3 [117]=3)"
    literal_transitions[12]="([100]=3)"
    literal_transitions[15]="([38]=3)"
    literal_transitions[16]="([74]=17)"
    literal_transitions[18]="([9]=3 [63]=3 [14]=3 [132]=3)"
    literal_transitions[19]="([74]=20)"
    literal_transitions[23]="([74]=24)"
    literal_transitions[24]="([41]=3)"
    literal_transitions[25]="([34]=6 [55]=6)"
    literal_transitions[26]="([74]=25)"
    literal_transitions[27]="([18]=3 [8]=3)"
    literal_transitions[28]="([133]=3 [93]=3)"
    literal_transitions[30]="([74]=33)"

    local -A match_anything_transitions
    match_anything_transitions=([2]=3 [28]=3 [11]=32 [31]=23 [15]=26 [8]=8 [3]=26 [29]=30 [17]=22 [13]=3 [32]=16 [1]=8 [20]=29 [21]=3 [7]=3 [33]=31 [14]=2 [12]=26 [22]=19 [5]=28)

    declare -A subword_transitions

    local state=1
    local word_index=2
    while [[ $word_index -lt $CURRENT ]]; do
        if [[ -v "literal_transitions[$state]" ]]; then
            local -A state_transitions
            eval "state_transitions=${literal_transitions[$state]}"

            local word=${words[$word_index]}
            local word_matched=0
            for ((literal_id = 1; literal_id <= $#literals; literal_id++)); do
                if [[ ${literals[$literal_id]} = "$word" ]]; then
                    if [[ -v "state_transitions[$literal_id]" ]]; then
                        state=${state_transitions[$literal_id]}
                        word_index=$((word_index + 1))
                        word_matched=1
                        break
                    fi
                fi
            done
            if [[ $word_matched -ne 0 ]]; then
                continue
            fi
        fi

        if [[ -v "match_anything_transitions[$state]" ]]; then
            state=${match_anything_transitions[$state]}
            word_index=$((word_index + 1))
            continue
        fi

        return 1
    done

    completions_no_description_trailing_space=()
    completions_no_description_no_trailing_space=()
    completions_trailing_space=()
    suffixes_trailing_space=()
    descriptions_trailing_space=()
    completions_no_trailing_space=()
    suffixes_no_trailing_space=()
    descriptions_no_trailing_space=()

    if [[ -v "literal_transitions[$state]" ]]; then
        local -A state_transitions
        eval "state_transitions=${literal_transitions[$state]}"

        for literal_id in ${(k)state_transitions}; do
            if [[ -v "descriptions[$literal_id]" ]]; then
                completions_trailing_space+=("${literals[$literal_id]}")
                suffixes_trailing_space+=("${literals[$literal_id]}")
                descriptions_trailing_space+=("${descriptions[$literal_id]}")
            else
                completions_no_description_trailing_space+=("${literals[$literal_id]}")
            fi
        done
    fi
    local -A commands=([33]=3 [17]=1 [20]=2 [11]=0)

    if [[ -v "commands[$state]" ]]; then
        local command_id=${commands[$state]}
        local output=$(_hyprctl_cmd_${command_id} "${words[$CURRENT]}")
        local -a command_completions=("${(@f)output}")
        for line in ${command_completions[@]}; do
            local parts=(${(@s:	:)line})
            if [[ -v "parts[2]" ]]; then
                completions_trailing_space+=("${parts[1]}")
                suffixes_trailing_space+=("${parts[1]}")
                descriptions_trailing_space+=("${parts[2]}")
            else
                completions_no_description_trailing_space+=("${parts[1]}")
            fi
        done
    fi

    local maxlen=0
    for suffix in ${suffixes_trailing_space[@]}; do
        if [[ ${#suffix} -gt $maxlen ]]; then
            maxlen=${#suffix}
        fi
    done
    for suffix in ${suffixes_no_trailing_space[@]}; do
        if [[ ${#suffix} -gt $maxlen ]]; then
            maxlen=${#suffix}
        fi
    done

    for ((i = 1; i <= $#suffixes_trailing_space; i++)); do
        if [[ -z ${descriptions_trailing_space[$i]} ]]; then
            descriptions_trailing_space[$i]="${(r($maxlen)( ))${suffixes_trailing_space[$i]}}"
        else
            descriptions_trailing_space[$i]="${(r($maxlen)( ))${suffixes_trailing_space[$i]}} -- ${descriptions_trailing_space[$i]}"
        fi
    done

    for ((i = 1; i <= $#suffixes_no_trailing_space; i++)); do
        if [[ -z ${descriptions_no_trailing_space[$i]} ]]; then
            descriptions_no_trailing_space[$i]="${(r($maxlen)( ))${suffixes_no_trailing_space[$i]}}"
        else
            descriptions_no_trailing_space[$i]="${(r($maxlen)( ))${suffixes_no_trailing_space[$i]}} -- ${descriptions_no_trailing_space[$i]}"
        fi
    done

    compadd -Q -a completions_no_description_trailing_space
    compadd -Q -S ' ' -a completions_no_description_no_trailing_space
    compadd -l -Q -a -d descriptions_trailing_space completions_trailing_space
    compadd -l -Q -S '' -a -d descriptions_no_trailing_space completions_no_trailing_space
    return 0
}

compdef _hyprctl hyprctl
