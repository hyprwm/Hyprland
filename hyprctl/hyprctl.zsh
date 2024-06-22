#compdef hyprctl

_hyprctl_cmd_2 () {
    hyprctl monitors | grep Monitor | awk '{ print $2 }'
}

_hyprctl_cmd_3 () {
    hyprpm list | grep "Plugin" | awk '{print $4}'
}

_hyprctl_cmd_0 () {
    hyprctl clients | grep class | awk '{print $2}'
}

_hyprctl_cmd_1 () {
    hyprctl devices | sed -n '/Keyboard at/{n; s/^\s\+//; p}'
}

_hyprctl () {
    local -a literals=("cyclenext" "globalshortcuts" "cursorpos" "bordersize" "renameworkspace" "animationstyle" "focuswindow" "0" "auto" "swapnext" "forceallowsinput" "moveactive" "activebordercolor" "alphafullscreen" "wayland" "layers" "minsize" "monitors" "1" "kill" "settiled" "3" "focusmonitor" "swapwindow" "moveoutofgroup" "notify" "movecursor" "setcursor" "seterror" "movecurrentworkspacetomonitor" "4" "nomaxsize" "forcenoanims" "setprop" "-i" "-q" "togglefloating" "workspacerules" "movetoworkspace" "disable" "setignoregrouplock" "workspaces" "movegroupwindow" "closewindow" "0" "--instance" "binds" "movewindow" "splitratio" "alpha" "denywindowfromgroup" "workspace" "configerrors" "togglegroup" "getoption" "forceopaque" "keepaspectratio" "killactive" "pass" "decorations" "devices" "focuscurrentorlast" "submap" "global" "alphafullscreenoverride" "forcerendererreload" "movewindowpixel" "headless" "version" "dpms" "resizeactive" "moveintogroup" "5" "alphaoverride" "setfloating" "rollinglog" "::=" "rounding" "layouts" "moveworkspacetomonitor" "exec" "alphainactiveoverride" "alterzorder" "fakefullscreen" "nofocus" "keyword" "forcenoborder" "forcenodim" "--quiet" "pin" "output" "forcenoblur" "togglespecialworkspace" "fullscreen" "toggleopaque" "focusworkspaceoncurrentmonitor" "next" "changegroupactive" "-j" "instances" "execr" "exit" "clients" "all" "--batch" "dismissnotify" "inactivebordercolor" "switchxkblayout" "movetoworkspacesilent" "tagwindow" "movewindoworgroup" "-r" "movefocus" "focusurgentorlast" "remove" "activeworkspace" "dispatch" "create" "centerwindow" "2" "hyprpaper" "-1" "reload" "alphainactive" "systeminfo" "plugin" "dimaround" "activewindow" "swapactiveworkspaces" "splash" "sendshortcut" "maxsize" "lockactivegroup" "windowdancecompat" "forceopaqueoverriden" "lockgroups" "movecursortocorner" "x11" "prev" "1" "resizewindowpixel" "forcenoshadow")

    local -A descriptions
    descriptions[1]="Focus the next window on a workspace"
    descriptions[3]="Get the current cursor pos in global layout coordinates"
    descriptions[5]="Rename a workspace"
    descriptions[7]="Focus the first window matching"
    descriptions[10]="Swap the focused window with the next window"
    descriptions[12]="Move the active window"
    descriptions[16]="List the layers"
    descriptions[18]="List active outputs with their properties"
    descriptions[20]="Get into a kill mode, where you can kill an app by clicking on it"
    descriptions[21]="Set the current window's floating state to false"
    descriptions[22]="ERROR"
    descriptions[23]="Focus a monitor"
    descriptions[24]="Swap the active window with another window"
    descriptions[25]="Move the active window out of a group"
    descriptions[26]="Send a notification using the built-in Hyprland notification system"
    descriptions[27]="Move the cursor to a specified position"
    descriptions[28]="Set the cursor theme and reloads the cursor manager"
    descriptions[29]="Set the hyprctl error string"
    descriptions[30]="Move the active workspace to a monitor"
    descriptions[31]="CONFUSED"
    descriptions[34]="Set a property of a window"
    descriptions[35]="Specify the Hyprland instance"
    descriptions[36]="Disable output"
    descriptions[37]="Toggle the current window's floating state"
    descriptions[38]="Get the list of defined workspace rules"
    descriptions[39]="Move the focused window to a workspace"
    descriptions[41]="Temporarily enable or disable binds:ignore_group_lock"
    descriptions[42]="List all workspaces with their properties"
    descriptions[43]="Swap the active window with the next or previous in a group"
    descriptions[44]="Close a specified window"
    descriptions[45]="WARNING"
    descriptions[46]="Specify the Hyprland instance"
    descriptions[47]="List all registered binds"
    descriptions[48]="Move the active window in a direction or to a monitor"
    descriptions[49]="Change the split ratio"
    descriptions[51]="Prohibit the active window from becoming or being inserted into group"
    descriptions[52]="Change the workspace"
    descriptions[53]="List all current config parsing errors"
    descriptions[54]="Toggle the current active window into a group"
    descriptions[55]="Get the config option status (values)"
    descriptions[58]="Close the active window"
    descriptions[59]="Pass the key to a specified window"
    descriptions[60]="List all decorations and their info"
    descriptions[61]="List all connected keyboards and mice"
    descriptions[62]="Switch focus from current to previously focused window"
    descriptions[63]="Change the current mapping group"
    descriptions[64]="Execute a Global Shortcut using the GlobalShortcuts portal"
    descriptions[66]="Force the renderer to reload all resources and outputs"
    descriptions[67]="Move a selected window"
    descriptions[69]="Print the Hyprland version: flags, commit and branch of build"
    descriptions[70]="Set all monitors' DPMS status"
    descriptions[71]="Resize the active window"
    descriptions[72]="Move the active window into a group"
    descriptions[73]="OK"
    descriptions[75]="Set the current window's floating state to true"
    descriptions[76]="Print tail of the log"
    descriptions[79]="List all layouts available (including plugin ones)"
    descriptions[80]="Move a workspace to a monitor"
    descriptions[81]="Execute a shell command"
    descriptions[83]="Modify the window stack order of the active or specified window"
    descriptions[84]="Toggle the focused window's internal fullscreen state"
    descriptions[86]="Issue a keyword to call a config keyword dynamically"
    descriptions[89]="Disable output"
    descriptions[90]="Pin a window"
    descriptions[91]="Allows adding/removing fake outputs to a specific backend"
    descriptions[93]="Toggle a special workspace on/off"
    descriptions[94]="Toggle the focused window's fullscreen state"
    descriptions[95]="Toggle the current window to always be opaque"
    descriptions[96]="Focus the requested workspace"
    descriptions[98]="Switch to the next window in a group"
    descriptions[99]="Output in JSON format"
    descriptions[100]="List all running Hyprland instances and their info"
    descriptions[101]="Execute a raw shell command"
    descriptions[102]="Exit the compositor with no questions asked"
    descriptions[103]="List all windows with their properties"
    descriptions[105]="Execute a batch of commands separated by ;"
    descriptions[106]="Dismiss all or up to amount of notifications"
    descriptions[108]="Set the xkb layout index for a keyboard"
    descriptions[109]="Move window doesnt switch to the workspace"
    descriptions[110]="Apply a tag to the window"
    descriptions[111]="Behave as moveintogroup"
    descriptions[112]="Refresh state after issuing the command"
    descriptions[113]="Move the focus in a direction"
    descriptions[114]="Focus the urgent window or the last window"
    descriptions[116]="Get the active workspace name and its properties"
    descriptions[117]="Issue a dispatch to call a keybind dispatcher with an arg"
    descriptions[119]="Center the active window"
    descriptions[120]="HINT"
    descriptions[121]="Interact with hyprpaper if present"
    descriptions[122]="No Icon"
    descriptions[123]="Force reload the config"
    descriptions[125]="Print system info"
    descriptions[126]="Interact with a plugin"
    descriptions[128]="Get the active window name and its properties"
    descriptions[129]="Swap the active workspaces between two monitors"
    descriptions[130]="Print the current random splash"
    descriptions[131]="On shortcut X sends shortcut Y to a specified window"
    descriptions[133]="Lock the focused group"
    descriptions[136]="Lock the groups"
    descriptions[137]="Move the cursor to the corner of the active window"
    descriptions[140]="INFO"
    descriptions[141]="Resize a selected window"

    local -A literal_transitions
    literal_transitions[1]="([106]=2 [76]=3 [34]=4 [36]=5 [2]=3 [3]=3 [79]=3 [108]=6 [38]=3 [112]=5 [42]=3 [47]=3 [116]=3 [86]=7 [117]=9 [53]=3 [89]=5 [55]=3 [91]=10 [121]=3 [123]=3 [125]=3 [16]=3 [60]=11 [61]=3 [18]=12 [126]=13 [20]=3 [128]=3 [130]=3 [26]=14 [69]=3 [99]=5 [100]=3 [28]=3 [29]=15 [103]=3 [105]=5)"
    literal_transitions[4]="([74]=18 [14]=3 [33]=18 [56]=18 [57]=18 [92]=18 [107]=3 [124]=3 [78]=2 [17]=3 [127]=18 [4]=2 [6]=3 [65]=18 [132]=3 [134]=18 [82]=18 [135]=18 [85]=18 [32]=18 [50]=3 [13]=3 [87]=18 [11]=18 [88]=18 [142]=18)"
    literal_transitions[8]="([106]=2 [76]=3 [34]=4 [2]=3 [3]=3 [79]=3 [108]=6 [38]=3 [42]=3 [47]=3 [116]=3 [86]=7 [117]=9 [53]=3 [55]=3 [91]=10 [121]=3 [123]=3 [125]=3 [16]=3 [60]=11 [61]=3 [18]=12 [126]=13 [20]=3 [128]=3 [130]=3 [26]=14 [69]=3 [100]=3 [28]=3 [29]=15 [103]=3)"
    literal_transitions[9]="([102]=3 [131]=3 [133]=3 [1]=3 [75]=3 [37]=3 [109]=3 [110]=3 [39]=3 [111]=3 [5]=3 [80]=3 [41]=3 [81]=3 [114]=3 [7]=3 [43]=3 [44]=3 [83]=3 [84]=3 [48]=3 [49]=3 [10]=3 [51]=3 [52]=3 [54]=3 [12]=3 [113]=3 [90]=3 [119]=3 [58]=3 [93]=3 [59]=3 [94]=3 [95]=3 [62]=3 [63]=3 [129]=3 [96]=3 [64]=3 [21]=3 [98]=3 [23]=3 [24]=3 [66]=3 [67]=3 [136]=3 [137]=3 [25]=3 [27]=3 [70]=3 [101]=3 [71]=3 [141]=3 [30]=3 [72]=3)"
    literal_transitions[10]="([118]=21 [115]=17)"
    literal_transitions[12]="([104]=3)"
    literal_transitions[14]="([22]=2 [120]=2 [31]=2 [140]=2 [122]=2 [45]=2 [73]=2)"
    literal_transitions[15]="([40]=3)"
    literal_transitions[16]="([139]=3 [97]=3)"
    literal_transitions[18]="([19]=3 [8]=3)"
    literal_transitions[19]="([77]=20)"
    literal_transitions[20]="([35]=5 [46]=5)"
    literal_transitions[21]="([9]=3 [68]=3 [15]=3 [138]=3)"

    local -A match_anything_transitions
    match_anything_transitions=([2]=3 [1]=8 [7]=3 [16]=3 [11]=3 [6]=16 [15]=19 [8]=8 [3]=19 [17]=3 [13]=3 [12]=19)

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
    local -A commands=([6]=1 [17]=2 [13]=3 [11]=0)

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
