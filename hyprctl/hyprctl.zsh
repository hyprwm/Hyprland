#compdef hyprctl

_hyprctl_cmd_3 () {
    hyprctl monitors | grep Monitor | awk '{ print $2 }'
}

_hyprctl_cmd_2 () {
    hyprpm list | grep "Plugin" | awk '{print $4}'
}

_hyprctl_cmd_0 () {
    hyprctl devices | sed -n '/Keyboard at/{n; s/^\s\+//; p}'
}

_hyprctl_cmd_1 () {
    hyprctl clients | grep class | awk '{print $2}'
}

_hyprctl () {
    local -a literals=("focusmonitor" "exit" "global" "forceallowsinput" "::=" "movecursortocorner" "movewindowpixel" "activeworkspace" "monitors" "movecurrentworkspacetomonitor" "togglespecialworkspace" "all" "animationstyle" "closewindow" "setprop" "clients" "denywindowfromgroup" "create" "moveoutofgroup" "headless" "activebordercolor" "rollinglog" "wayland" "movewindoworgroup" "setcursor" "fakefullscreen" "moveactive" "prev" "hyprpaper" "alpha" "inactivebordercolor" "-i" "--instance" "togglefloating" "settiled" "swapwindow" "dimaround" "setignoregrouplock" "layouts" "0" "forcenoborder" "notify" "binds" "focuswindow" "seterror" "1" "systeminfo" "exec" "cyclenext" "nomaxsize" "reload" "rounding" "layers" "setfloating" "5" "lockactivegroup" "movetoworkspace" "swapactiveworkspaces" "changegroupactive" "forcenodim" "0" "configerrors" "4" "forceopaque" "forcenoshadow" "workspaces" "1" "swapnext" "minsize" "alphaoverride" "toggleopaque" "decorations" "alterzorder" "bordersize" "-1" "focuscurrentorlast" "workspacerules" "splitratio" "remove" "renameworkspace" "movetoworkspacesilent" "killactive" "pass" "getoption" "switchxkblayout" "2" "auto" "pin" "version" "nofocus" "togglegroup" "workspace" "lockgroups" "-r" "movewindow" "cursorpos" "focusworkspaceoncurrentmonitor" "execr" "windowdancecompat" "globalshortcuts" "3" "keyword" "movefocus" "movecursor" "instances" "dpms" "x11" "moveintogroup" "resizewindowpixel" "kill" "moveworkspacetomonitor" "forceopaqueoverriden" "dispatch" "-j" "forcenoblur" "devices" "disable" "-b" "activewindow" "fullscreen" "keepaspectratio" "output" "plugin" "alphainactiveoverride" "alphainactive" "resizeactive" "centerwindow" "splash" "focusurgentorlast" "submap" "next" "movegroupwindow" "forcenoanims" "forcerendererreload" "maxsize" "dismissnotify")

    local -A descriptions
    descriptions[1]="focuses a monitor"
    descriptions[2]="exits the compositor with no questions asked"
    descriptions[3]="Executes a Global Shortcut using the GlobalShortcuts portal"
    descriptions[6]="moves the cursor to the corner of the active window"
    descriptions[7]="moves a selected window	resizeparams"
    descriptions[8]="Gets the active workspace name and its properties"
    descriptions[9]="lists active outputs with their properties"
    descriptions[10]="Moves the active workspace to a monitor"
    descriptions[11]="toggles a special workspace on/off"
    descriptions[14]="closes a specified window"
    descriptions[15]="Sets a property of a window"
    descriptions[16]="Lists all windows with their properties"
    descriptions[17]="Prohibit the active window from becoming or being inserted into group"
    descriptions[19]="Moves the active window out of a group"
    descriptions[22]="Prints tail of the log"
    descriptions[24]="Behaves as moveintogroup"
    descriptions[25]="Sets the cursor theme and reloads the cursor manager"
    descriptions[26]="toggles the focused window’s internal fullscreen state"
    descriptions[27]="moves the active window	resizeparams"
    descriptions[29]="Interact with hyprpaper if present"
    descriptions[32]="Specify the Hyprland instalnce"
    descriptions[33]="Specify the Hyprland instalnce"
    descriptions[34]="toggles the current window’s floating state"
    descriptions[35]="sets the current window’s floating state to false"
    descriptions[36]="swaps the active window with another window"
    descriptions[38]="Temporarily enable or disable binds:ignore_group_lock"
    descriptions[39]="lists all layouts available (including plugin'd ones)"
    descriptions[42]="Sends a notification using the built-in Hyprland notification system"
    descriptions[43]="Lists all registered binds"
    descriptions[44]="focuses the first window matching"
    descriptions[45]="Sets the hyprctl error string"
    descriptions[47]="Prints system info"
    descriptions[48]="executes a shell command"
    descriptions[49]="focuses the next window on a workspace"
    descriptions[51]="Force reloads the config"
    descriptions[53]="List the layers"
    descriptions[54]="sets the current window’s floating state to true"
    descriptions[55]="OK"
    descriptions[56]="Lock the focused group"
    descriptions[57]="moves the focused window to a workspace"
    descriptions[58]="Swaps the active workspaces between two monitors"
    descriptions[59]="switches to the next window in a group"
    descriptions[61]="WARNING"
    descriptions[62]="Lists all current config parsing errors"
    descriptions[63]="CONFISED"
    descriptions[66]="Lists all workspaces with their properties"
    descriptions[67]="INFOROR"
    descriptions[68]="swaps the focused window with the next window"
    descriptions[71]="toggles the current window to always be opaque"
    descriptions[72]="Lists all decorations and their info"
    descriptions[73]="Modify the window stack order of the active or specified window"
    descriptions[75]="No Icon"
    descriptions[76]="Switch focus from current to previously focused window"
    descriptions[77]="Gets the list of defined workspace rules"
    descriptions[78]="changes the split ratio"
    descriptions[80]="rename a workspace"
    descriptions[81]="move window doesnt switch to the workspace"
    descriptions[82]="closes the active window"
    descriptions[83]="passes the key to a specified window"
    descriptions[84]="Gets the config option status (values)"
    descriptions[85]="Sets the xkb layout index for a keyboard"
    descriptions[86]="HINT"
    descriptions[88]="pins a window"
    descriptions[89]="Prints the Hyprland version, meaning flags, commit and branch of build"
    descriptions[91]="toggles the current active window into a group"
    descriptions[92]="changes the workspace"
    descriptions[93]="Locks the groups"
    descriptions[94]="Refresh state befor issuing the command"
    descriptions[95]="moves the active window in a direction or to a monitor"
    descriptions[96]="Gets the current cursor pos in global layout coordinates"
    descriptions[97]="Focuses the requested workspace"
    descriptions[98]="executes a raw shell command"
    descriptions[101]="ERROR"
    descriptions[102]="Issue a keyword to call a config keyword dynamically"
    descriptions[103]="moves the focus in a direction"
    descriptions[104]="moves the cursor to a specified position"
    descriptions[105]="Lists all running instances of Hyprland with thir info"
    descriptions[106]="sets all monitors’ DPMS status"
    descriptions[108]="Moves the active window into a group"
    descriptions[109]="resizes a selected window"
    descriptions[110]="Get into a kill mode, where you can kill an app by clicking on it"
    descriptions[111]="Moves a workspace to a monitor"
    descriptions[113]="Issue a dispatch to call a keybind dispatcher with an arg"
    descriptions[114]="Output in JSON format"
    descriptions[116]="Lists all connected keyboards and mice"
    descriptions[118]="Execute a batch of commands separated by ;"
    descriptions[119]="Gets the active window name and its properties"
    descriptions[120]="toggles the focused window’s fullscreen state"
    descriptions[122]="Allows you to add and remove fake outputs to your preferred backend"
    descriptions[123]="Interact with a plugin"
    descriptions[126]="resizes the active window"
    descriptions[127]="center the active window"
    descriptions[128]="Prints the current random splash"
    descriptions[129]="Focuses the urgent window or the last window"
    descriptions[130]="Change the current mapping group"
    descriptions[132]="Swaps the active window with the next or previous in a group"
    descriptions[134]="forces the renderer to reload all resources and outputs"
    descriptions[136]="Dismisses all or up to amount of notifications"

    local -A literal_transitions
    literal_transitions[1]="([45]=32 [116]=4 [47]=4 [85]=22 [84]=4 [118]=12 [51]=4 [53]=4 [119]=4 [8]=4 [9]=29 [122]=26 [15]=8 [89]=4 [16]=4 [123]=25 [62]=4 [94]=12 [22]=4 [25]=4 [66]=4 [96]=4 [29]=4 [100]=4 [128]=4 [102]=31 [72]=19 [105]=4 [39]=4 [110]=4 [77]=4 [114]=12 [113]=28 [42]=2 [43]=4 [136]=3)"
    literal_transitions[7]="([87]=4 [20]=4 [107]=4 [23]=4)"
    literal_transitions[8]="([125]=4 [64]=9 [65]=9 [50]=9 [99]=9 [31]=4 [30]=4 [52]=3 [4]=9 [69]=4 [70]=9 [121]=9 [13]=4 [74]=3 [37]=9 [133]=9 [90]=9 [60]=9 [112]=9 [41]=9 [21]=4 [124]=9 [135]=4 [115]=9)"
    literal_transitions[9]="([40]=4 [46]=4)"
    literal_transitions[10]="([5]=11)"
    literal_transitions[11]="([33]=12 [32]=12)"
    literal_transitions[13]="([28]=4 [131]=4)"
    literal_transitions[16]="([5]=5)"
    literal_transitions[17]="([5]=18)"
    literal_transitions[18]="([61]=4)"
    literal_transitions[19]="([63]=4 [86]=4 [101]=4 [67]=4 [75]=4 [55]=4)"
    literal_transitions[21]="([5]=14)"
    literal_transitions[26]="([18]=7 [79]=27)"
    literal_transitions[28]="([1]=4 [2]=4 [48]=4 [49]=4 [3]=4 [81]=4 [82]=4 [83]=4 [6]=4 [7]=4 [54]=4 [56]=4 [11]=4 [10]=4 [57]=4 [120]=4 [14]=4 [88]=4 [17]=4 [58]=4 [19]=4 [59]=4 [91]=4 [95]=4 [97]=4 [92]=4 [24]=4 [93]=4 [26]=4 [27]=4 [68]=4 [98]=4 [126]=4 [127]=4 [34]=4 [71]=4 [103]=4 [35]=4 [104]=4 [73]=4 [36]=4 [129]=4 [130]=4 [76]=4 [38]=4 [106]=4 [108]=4 [109]=4 [78]=4 [111]=4 [132]=4 [44]=4 [134]=4 [80]=4)"
    literal_transitions[29]="([12]=4)"
    literal_transitions[30]="([43]=4 [116]=4 [45]=32 [51]=4 [84]=4 [85]=22 [53]=4 [119]=4 [8]=4 [9]=29 [122]=26 [15]=8 [89]=4 [16]=4 [123]=25 [62]=4 [22]=4 [25]=4 [66]=4 [96]=4 [29]=4 [100]=4 [128]=4 [102]=31 [72]=19 [105]=4 [39]=4 [110]=4 [77]=4 [113]=28 [42]=2 [47]=4 [136]=3)"
    literal_transitions[32]="([117]=4)"
    literal_transitions[33]="([5]=23)"

    local -A match_anything_transitions
    match_anything_transitions=([1]=30 [5]=6 [24]=16 [3]=4 [19]=20 [29]=10 [32]=10 [25]=4 [30]=30 [14]=15 [15]=33 [2]=3 [23]=24 [20]=21 [6]=17 [13]=4 [31]=4 [4]=10 [22]=13 [27]=4)

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
    local -A commands=([5]=0 [14]=2 [23]=3 [19]=1)

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

if [[ $ZSH_EVAL_CONTEXT =~ :file$ ]]; then
    compdef _hyprctl hyprctl
else
    _hyprctl
fi
