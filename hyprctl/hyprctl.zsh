#compdef hyprctl

_hyprctl_cmd_2 () {
    hyprctl monitors | grep Monitor | awk '{ print $2 }'
}

_hyprctl_cmd_0 () {
    hyprpm list | grep "Plugin" | awk '{print $4}'
}

_hyprctl_cmd_1 () {
    hyprctl devices | sed -n '/Keyboard at/{n; s/^\s\+//; p}'
}

_hyprctl_cmd_3 () {
    hyprctl clients | grep class | awk '{print $2}'
}

_hyprctl () {
    local -a literals=("resizeactive" "changegroupactive" "-r" "moveintogroup" "forceallowsinput" "4" "::=" "systeminfo" "all" "layouts" "animationstyle" "setprop" "switchxkblayout" "create" "denywindowfromgroup" "headless" "activebordercolor" "exec" "setcursor" "wayland" "focusurgentorlast" "workspacerules" "movecurrentworkspacetomonitor" "movetoworkspacesilent" "hyprpaper" "alpha" "inactivebordercolor" "movegroupwindow" "movecursortocorner" "movewindowpixel" "prev" "movewindow" "clients" "dimaround" "setignoregrouplock" "splash" "execr" "monitors" "0" "forcenoborder" "1" "nomaxsize" "splitratio" "moveactive" "pass" "swapnext" "devices" "layers" "rounding" "lockactivegroup" "5" "moveworkspacetomonitor" "-i" "forcenodim" "pin" "0" "1" "forceopaque" "forcenoshadow" "setfloating" "minsize" "alphaoverride" "workspaces" "cyclenext" "alterzorder" "togglegroup" "lockgroups" "bordersize" "dpms" "focuscurrentorlast" "-1" "--batch" "notify" "remove" "instances" "moveoutofgroup" "killactive" "2" "movetoworkspace" "movecursor" "configerrors" "closewindow" "swapwindow" "auto" "forcerendererreload" "centerwindow" "focuswindow" "seterror" "nofocus" "version" "binds" "togglespecialworkspace" "fullscreen" "windowdancecompat" "globalshortcuts" "keyword" "toggleopaque" "3" "--instance" "togglefloating" "renameworkspace" "activeworkspace" "x11" "kill" "forceopaqueoverriden" "output" "global" "dispatch" "reload" "forcenoblur" "-j" "disable" "activewindow" "keepaspectratio" "dismissnotify" "focusmonitor" "movefocus" "plugin" "exit" "workspace" "getoption" "alphainactiveoverride" "alphainactive" "decorations" "settiled" "resizewindowpixel" "fakefullscreen" "rollinglog" "swapactiveworkspaces" "submap" "next" "movewindoworgroup" "cursorpos" "forcenoanims" "focusworkspaceoncurrentmonitor" "maxsize")

    local -A descriptions
    descriptions[1]="Resize the active window"
    descriptions[2]="Switch to the next window in a group"
    descriptions[3]="Refresh state after issuing the command"
    descriptions[4]="Move the active window into a group"
    descriptions[6]="CONFUSED"
    descriptions[8]="Print system info"
    descriptions[10]="List all layouts available (including plugin ones)"
    descriptions[12]="Set a property of a window"
    descriptions[13]="Set the xkb layout index for a keyboard"
    descriptions[15]="Prohibit the active window from becoming or being inserted into group"
    descriptions[18]="Execute a shell command"
    descriptions[19]="Set the cursor theme and reloads the cursor manager"
    descriptions[21]="Focus the urgent window or the last window"
    descriptions[22]="Get the list of defined workspace rules"
    descriptions[23]="Move the active workspace to a monitor"
    descriptions[24]="Move window doesnt switch to the workspace"
    descriptions[25]="Interact with hyprpaper if present"
    descriptions[28]="Swap the active window with the next or previous in a group"
    descriptions[29]="Move the cursor to the corner of the active window"
    descriptions[30]="Move a selected window"
    descriptions[32]="Move the active window in a direction or to a monitor"
    descriptions[33]="List all windows with their properties"
    descriptions[35]="Temporarily enable or disable binds:ignore_group_lock"
    descriptions[36]="Print the current random splash"
    descriptions[37]="Execute a raw shell command"
    descriptions[38]="List active outputs with their properties"
    descriptions[43]="Change the split ratio"
    descriptions[44]="Move the active window"
    descriptions[45]="Pass the key to a specified window"
    descriptions[46]="Swap the focused window with the next window"
    descriptions[47]="List all connected keyboards and mice"
    descriptions[48]="List the layers"
    descriptions[50]="Lock the focused group"
    descriptions[51]="OK"
    descriptions[52]="Move a workspace to a monitor"
    descriptions[53]="Specify the Hyprland instance"
    descriptions[55]="Pin a window"
    descriptions[56]="WARNING"
    descriptions[57]="INFO"
    descriptions[60]="Set the current window's floating state to true"
    descriptions[63]="List all workspaces with their properties"
    descriptions[64]="Focus the next window on a workspace"
    descriptions[65]="Modify the window stack order of the active or specified window"
    descriptions[66]="Toggle the current active window into a group"
    descriptions[67]="Lock the groups"
    descriptions[69]="Set all monitors' DPMS status"
    descriptions[70]="Switch focus from current to previously focused window"
    descriptions[71]="No Icon"
    descriptions[72]="Execute a batch of commands separated by ;"
    descriptions[73]="Send a notification using the built-in Hyprland notification system"
    descriptions[75]="List all running Hyprland instances and their info"
    descriptions[76]="Move the active window out of a group"
    descriptions[77]="Close the active window"
    descriptions[78]="HINT"
    descriptions[79]="Move the focused window to a workspace"
    descriptions[80]="Move the cursor to a specified position"
    descriptions[81]="List all current config parsing errors"
    descriptions[82]="Close a specified window"
    descriptions[83]="Swap the active window with another window"
    descriptions[85]="Force the renderer to reload all resources and outputs"
    descriptions[86]="Center the active window"
    descriptions[87]="Focus the first window matching"
    descriptions[88]="Set the hyprctl error string"
    descriptions[90]="Print the Hyprland version: flags, commit and branch of build"
    descriptions[91]="List all registered binds"
    descriptions[92]="Toggle a special workspace on/off"
    descriptions[93]="Toggle the focused window's fullscreen state"
    descriptions[96]="Issue a keyword to call a config keyword dynamically"
    descriptions[97]="Toggle the current window to always be opaque"
    descriptions[98]="ERROR"
    descriptions[99]="Specify the Hyprland instance"
    descriptions[100]="Toggle the current window's floating state"
    descriptions[101]="Rename a workspace"
    descriptions[102]="Get the active workspace name and its properties"
    descriptions[104]="Get into a kill mode, where you can kill an app by clicking on it"
    descriptions[106]="Allows adding/removing fake outputs to a specific backend"
    descriptions[107]="Execute a Global Shortcut using the GlobalShortcuts portal"
    descriptions[108]="Issue a dispatch to call a keybind dispatcher with an arg"
    descriptions[109]="Force reload the config"
    descriptions[111]="Output in JSON format"
    descriptions[113]="Get the active window name and its properties"
    descriptions[115]="Dismiss all or up to amount of notifications"
    descriptions[116]="Focus a monitor"
    descriptions[117]="Move the focus in a direction"
    descriptions[118]="Interact with a plugin"
    descriptions[119]="Exit the compositor with no questions asked"
    descriptions[120]="Change the workspace"
    descriptions[121]="Get the config option status (values)"
    descriptions[124]="List all decorations and their info"
    descriptions[125]="Set the current window's floating state to false"
    descriptions[126]="Resize a selected window"
    descriptions[127]="Toggle the focused window's internal fullscreen state"
    descriptions[128]="Print tail of the log"
    descriptions[129]="Swap the active workspaces between two monitors"
    descriptions[130]="Change the current mapping group"
    descriptions[132]="Behave as moveintogroup"
    descriptions[133]="Get the current cursor pos in global layout coordinates"
    descriptions[135]="Focus the requested workspace"

    local -A literal_transitions
    literal_transitions[1]="([75]=3 [3]=20 [48]=3 [47]=3 [113]=3 [8]=3 [81]=3 [10]=3 [12]=4 [115]=5 [13]=6 [88]=7 [90]=3 [118]=8 [91]=3 [22]=3 [19]=3 [121]=3 [124]=9 [25]=3 [95]=3 [96]=10 [128]=3 [63]=3 [102]=3 [33]=3 [72]=20 [36]=3 [133]=3 [104]=3 [38]=11 [106]=12 [73]=14 [108]=13 [109]=3 [111]=20)"
    literal_transitions[2]="([48]=3 [47]=3 [113]=3 [8]=3 [81]=3 [10]=3 [12]=4 [115]=5 [13]=6 [88]=7 [90]=3 [118]=8 [91]=3 [22]=3 [19]=3 [121]=3 [124]=9 [25]=3 [95]=3 [96]=10 [128]=3 [63]=3 [102]=3 [33]=3 [36]=3 [104]=3 [38]=11 [106]=12 [109]=3 [108]=13 [73]=14 [133]=3 [75]=3)"
    literal_transitions[4]="([123]=3 [58]=15 [59]=15 [42]=15 [94]=15 [27]=3 [26]=3 [49]=5 [5]=15 [61]=3 [62]=15 [114]=15 [11]=3 [68]=5 [34]=15 [134]=15 [89]=15 [54]=15 [105]=15 [40]=15 [17]=3 [122]=15 [136]=3 [110]=15)"
    literal_transitions[7]="([112]=3)"
    literal_transitions[11]="([9]=3)"
    literal_transitions[12]="([14]=17 [74]=21)"
    literal_transitions[13]="([1]=3 [76]=3 [2]=3 [77]=3 [43]=3 [44]=3 [4]=3 [45]=3 [46]=3 [79]=3 [80]=3 [50]=3 [82]=3 [52]=3 [83]=3 [85]=3 [86]=3 [116]=3 [87]=3 [117]=3 [15]=3 [119]=3 [55]=3 [120]=3 [18]=3 [92]=3 [21]=3 [93]=3 [23]=3 [125]=3 [24]=3 [60]=3 [126]=3 [28]=3 [29]=3 [30]=3 [97]=3 [127]=3 [129]=3 [64]=3 [32]=3 [65]=3 [66]=3 [67]=3 [100]=3 [69]=3 [35]=3 [70]=3 [37]=3 [101]=3 [130]=3 [107]=3 [132]=3 [135]=3)"
    literal_transitions[14]="([78]=5 [6]=5 [98]=5 [56]=5 [71]=5 [51]=5 [57]=5)"
    literal_transitions[15]="([39]=3 [41]=3)"
    literal_transitions[16]="([31]=3 [131]=3)"
    literal_transitions[17]="([84]=3 [16]=3 [103]=3 [20]=3)"
    literal_transitions[18]="([7]=19)"
    literal_transitions[19]="([53]=20 [99]=20)"

    local -A match_anything_transitions
    match_anything_transitions=([16]=3 [7]=18 [8]=3 [1]=2 [6]=16 [5]=3 [3]=18 [21]=3 [9]=3 [10]=3 [11]=18 [2]=2)

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
    local -A commands=([8]=0 [21]=2 [9]=3 [6]=1)

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
