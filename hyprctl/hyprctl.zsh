#compdef hyprctl

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
    local -a literals=("cyclenext" "globalshortcuts" "cursorpos" "bordersize" "renameworkspace" "animationstyle" "focuswindow" "0" "auto" "swapnext" "forceallowsinput" "moveactive" "activebordercolor" "alphafullscreen" "wayland" "layers" "minsize" "monitors" "1" "kill" "settiled" "3" "focusmonitor" "swapwindow" "moveoutofgroup" "notify" "movecursor" "setcursor" "seterror" "movecurrentworkspacetomonitor" "4" "nomaxsize" "forcenoanims" "setprop" "-i" "togglefloating" "workspacerules" "movetoworkspace" "disable" "setignoregrouplock" "workspaces" "movegroupwindow" "closewindow" "0" "--instance" "binds" "movewindow" "splitratio" "alpha" "denywindowfromgroup" "workspace" "configerrors" "togglegroup" "getoption" "forceopaque" "keepaspectratio" "killactive" "pass" "decorations" "devices" "focuscurrentorlast" "submap" "global" "alphafullscreenoverride" "forcerendererreload" "movewindowpixel" "headless" "version" "dpms" "resizeactive" "moveintogroup" "5" "alphaoverride" "setfloating" "rollinglog" "::=" "rounding" "layouts" "moveworkspacetomonitor" "exec" "alphainactiveoverride" "alterzorder" "fakefullscreen" "nofocus" "keyword" "forcenoborder" "forcenodim" "pin" "output" "forcenoblur" "togglespecialworkspace" "fullscreen" "toggleopaque" "focusworkspaceoncurrentmonitor" "next" "changegroupactive" "-j" "instances" "execr" "exit" "clients" "all" "--batch" "dismissnotify" "inactivebordercolor" "switchxkblayout" "movetoworkspacesilent" "movewindoworgroup" "-r" "movefocus" "focusurgentorlast" "remove" "activeworkspace" "dispatch" "create" "centerwindow" "2" "hyprpaper" "-1" "reload" "alphainactive" "systeminfo" "plugin" "dimaround" "activewindow" "swapactiveworkspaces" "splash" "maxsize" "lockactivegroup" "windowdancecompat" "forceopaqueoverriden" "lockgroups" "movecursortocorner" "x11" "prev" "1" "resizewindowpixel" "forcenoshadow")

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
    descriptions[36]="Toggle the current window's floating state"
    descriptions[37]="Get the list of defined workspace rules"
    descriptions[38]="Move the focused window to a workspace"
    descriptions[40]="Temporarily enable or disable binds:ignore_group_lock"
    descriptions[41]="List all workspaces with their properties"
    descriptions[42]="Swap the active window with the next or previous in a group"
    descriptions[43]="Close a specified window"
    descriptions[44]="WARNING"
    descriptions[45]="Specify the Hyprland instance"
    descriptions[46]="List all registered binds"
    descriptions[47]="Move the active window in a direction or to a monitor"
    descriptions[48]="Change the split ratio"
    descriptions[50]="Prohibit the active window from becoming or being inserted into group"
    descriptions[51]="Change the workspace"
    descriptions[52]="List all current config parsing errors"
    descriptions[53]="Toggle the current active window into a group"
    descriptions[54]="Get the config option status (values)"
    descriptions[57]="Close the active window"
    descriptions[58]="Pass the key to a specified window"
    descriptions[59]="List all decorations and their info"
    descriptions[60]="List all connected keyboards and mice"
    descriptions[61]="Switch focus from current to previously focused window"
    descriptions[62]="Change the current mapping group"
    descriptions[63]="Execute a Global Shortcut using the GlobalShortcuts portal"
    descriptions[65]="Force the renderer to reload all resources and outputs"
    descriptions[66]="Move a selected window"
    descriptions[68]="Print the Hyprland version: flags, commit and branch of build"
    descriptions[69]="Set all monitors' DPMS status"
    descriptions[70]="Resize the active window"
    descriptions[71]="Move the active window into a group"
    descriptions[72]="OK"
    descriptions[74]="Set the current window's floating state to true"
    descriptions[75]="Print tail of the log"
    descriptions[78]="List all layouts available (including plugin ones)"
    descriptions[79]="Move a workspace to a monitor"
    descriptions[80]="Execute a shell command"
    descriptions[82]="Modify the window stack order of the active or specified window"
    descriptions[83]="Toggle the focused window's internal fullscreen state"
    descriptions[85]="Issue a keyword to call a config keyword dynamically"
    descriptions[88]="Pin a window"
    descriptions[89]="Allows adding/removing fake outputs to a specific backend"
    descriptions[91]="Toggle a special workspace on/off"
    descriptions[92]="Toggle the focused window's fullscreen state"
    descriptions[93]="Toggle the current window to always be opaque"
    descriptions[94]="Focus the requested workspace"
    descriptions[96]="Switch to the next window in a group"
    descriptions[97]="Output in JSON format"
    descriptions[98]="List all running Hyprland instances and their info"
    descriptions[99]="Execute a raw shell command"
    descriptions[100]="Exit the compositor with no questions asked"
    descriptions[101]="List all windows with their properties"
    descriptions[103]="Execute a batch of commands separated by ;"
    descriptions[104]="Dismiss all or up to amount of notifications"
    descriptions[106]="Set the xkb layout index for a keyboard"
    descriptions[107]="Move window doesnt switch to the workspace"
    descriptions[108]="Behave as moveintogroup"
    descriptions[109]="Refresh state after issuing the command"
    descriptions[110]="Move the focus in a direction"
    descriptions[111]="Focus the urgent window or the last window"
    descriptions[113]="Get the active workspace name and its properties"
    descriptions[114]="Issue a dispatch to call a keybind dispatcher with an arg"
    descriptions[116]="Center the active window"
    descriptions[117]="HINT"
    descriptions[118]="Interact with hyprpaper if present"
    descriptions[119]="No Icon"
    descriptions[120]="Force reload the config"
    descriptions[122]="Print system info"
    descriptions[123]="Interact with a plugin"
    descriptions[125]="Get the active window name and its properties"
    descriptions[126]="Swap the active workspaces between two monitors"
    descriptions[127]="Print the current random splash"
    descriptions[129]="Lock the focused group"
    descriptions[132]="Lock the groups"
    descriptions[133]="Move the cursor to the corner of the active window"
    descriptions[136]="INFO"
    descriptions[137]="Resize a selected window"

    local -A literal_transitions
    literal_transitions[1]="([104]=2 [75]=3 [34]=4 [2]=3 [3]=3 [78]=3 [106]=5 [37]=3 [109]=6 [41]=3 [46]=3 [113]=3 [85]=7 [114]=9 [52]=3 [54]=3 [89]=10 [118]=3 [120]=3 [122]=3 [16]=3 [59]=11 [60]=3 [18]=12 [123]=13 [20]=3 [125]=3 [127]=3 [26]=14 [68]=3 [97]=6 [98]=3 [28]=3 [29]=15 [101]=3 [103]=6)"
    literal_transitions[4]="([73]=19 [14]=3 [33]=19 [55]=19 [56]=19 [90]=19 [105]=3 [121]=3 [77]=2 [17]=3 [124]=19 [4]=2 [6]=3 [64]=19 [128]=3 [130]=19 [81]=19 [131]=19 [84]=19 [32]=19 [49]=3 [13]=3 [86]=19 [11]=19 [87]=19 [138]=19)"
    literal_transitions[8]="([104]=2 [75]=3 [34]=4 [2]=3 [3]=3 [78]=3 [106]=5 [37]=3 [41]=3 [46]=3 [113]=3 [85]=7 [114]=9 [52]=3 [54]=3 [89]=10 [118]=3 [120]=3 [122]=3 [16]=3 [59]=11 [60]=3 [18]=12 [123]=13 [20]=3 [125]=3 [127]=3 [26]=14 [68]=3 [98]=3 [28]=3 [29]=15 [101]=3)"
    literal_transitions[9]="([129]=3 [132]=3 [1]=3 [74]=3 [36]=3 [107]=3 [38]=3 [108]=3 [5]=3 [79]=3 [40]=3 [80]=3 [111]=3 [7]=3 [42]=3 [43]=3 [82]=3 [83]=3 [47]=3 [48]=3 [10]=3 [110]=3 [51]=3 [53]=3 [12]=3 [116]=3 [88]=3 [50]=3 [57]=3 [91]=3 [58]=3 [92]=3 [93]=3 [61]=3 [62]=3 [126]=3 [94]=3 [63]=3 [21]=3 [96]=3 [23]=3 [24]=3 [65]=3 [66]=3 [25]=3 [133]=3 [27]=3 [69]=3 [99]=3 [70]=3 [30]=3 [137]=3 [71]=3 [100]=3)"
    literal_transitions[10]="([115]=16 [112]=17)"
    literal_transitions[12]="([102]=3)"
    literal_transitions[14]="([22]=2 [117]=2 [31]=2 [136]=2 [119]=2 [44]=2 [72]=2)"
    literal_transitions[15]="([39]=3)"
    literal_transitions[16]="([9]=3 [67]=3 [15]=3 [134]=3)"
    literal_transitions[18]="([76]=20)"
    literal_transitions[19]="([19]=3 [8]=3)"
    literal_transitions[20]="([35]=6 [45]=6)"
    literal_transitions[21]="([135]=3 [95]=3)"

    local -A match_anything_transitions
    match_anything_transitions=([2]=3 [1]=8 [7]=3 [21]=3 [11]=3 [3]=18 [8]=8 [13]=3 [15]=18 [17]=3 [5]=21 [12]=18)

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
    local -A commands=([17]=2 [5]=3 [13]=1 [11]=0)

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
