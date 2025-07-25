#compdef hyprctl

_hyprctl_cmd_1 () {
    hyprctl monitors | awk '/Monitor/{ print $2 }'
}

_hyprctl_cmd_3 () {
    hyprctl clients | awk '/class/{print $2}'
}

_hyprctl_cmd_2 () {
    hyprctl devices | sed -n '/Keyboard at/{n; s/^\s\+//; p}'
}

_hyprctl_cmd_0 () {
    hyprpm list | awk '/Plugin/{print $4}'
}

_hyprctl () {
    local -a literals=("resizeactive" "2" "changegroupactive" "-r" "moveintogroup" "forceallowsinput" "4" "::=" "systeminfo" "all" "layouts" "setprop" "animationstyle" "switchxkblayout" "create" "denywindowfromgroup" "headless" "activebordercolor" "exec" "setcursor" "wayland" "focusurgentorlast" "workspacerules" "movecurrentworkspacetomonitor" "movetoworkspacesilent" "hyprpaper" "alpha" "inactivebordercolor" "movegroupwindow" "movecursortocorner" "movewindowpixel" "prev" "movewindow" "globalshortcuts" "clients" "dimaround" "setignoregrouplock" "splash" "execr" "monitors" "0" "forcenoborder" "-q" "animations" "1" "nomaxsize" "splitratio" "moveactive" "pass" "swapnext" "devices" "layers" "rounding" "lockactivegroup" "5" "moveworkspacetomonitor" "-f" "-i" "--quiet" "forcenodim" "pin" "0" "1" "forceopaque" "forcenoshadow" "setfloating" "minsize" "alphaoverride" "sendshortcut" "workspaces" "cyclenext" "alterzorder" "togglegroup" "lockgroups" "bordersize" "dpms" "focuscurrentorlast" "-1" "--batch" "notify" "remove" "instances" "1" "3" "moveoutofgroup" "killactive" "2" "movetoworkspace" "movecursor" "configerrors" "closewindow" "swapwindow" "tagwindow" "forcerendererreload" "centerwindow" "auto" "focuswindow" "seterror" "nofocus" "alphafullscreen" "binds" "version" "-h" "togglespecialworkspace" "fullscreen" "windowdancecompat" "0" "keyword" "toggleopaque" "3" "--instance" "togglefloating" "renameworkspace" "alphafullscreenoverride" "activeworkspace" "x11" "kill" "forceopaqueoverriden" "output" "global" "dispatch" "reload" "forcenoblur" "-j" "event" "--help" "disable" "-1" "activewindow" "keepaspectratio" "dismissnotify" "focusmonitor" "movefocus" "plugin" "exit" "workspace" "fullscreenstate" "getoption" "alphainactiveoverride" "alphainactive" "decorations" "settiled" "config-only" "descriptions" "resizewindowpixel" "fakefullscreen" "rollinglog" "swapactiveworkspaces" "submap" "next" "movewindoworgroup" "cursorpos" "forcenoanims" "focusworkspaceoncurrentmonitor" "maxsize" "sendkeystate")

    local -A descriptions
    descriptions[1]="Resize the active window"
    descriptions[2]="Fullscreen"
    descriptions[3]="Switch to the next window in a group"
    descriptions[4]="Refresh state after issuing the command"
    descriptions[5]="Move the active window into a group"
    descriptions[7]="CONFUSED"
    descriptions[9]="Print system info"
    descriptions[11]="List all layouts available (including plugin ones)"
    descriptions[12]="Set a property of a window"
    descriptions[14]="Set the xkb layout index for a keyboard"
    descriptions[16]="Prohibit the active window from becoming or being inserted into group"
    descriptions[19]="Execute a shell command"
    descriptions[20]="Set the cursor theme and reloads the cursor manager"
    descriptions[22]="Focus the urgent window or the last window"
    descriptions[23]="Get the list of defined workspace rules"
    descriptions[24]="Move the active workspace to a monitor"
    descriptions[25]="Move window doesn't switch to the workspace"
    descriptions[26]="Interact with hyprpaper if present"
    descriptions[29]="Swap the active window with the next or previous in a group"
    descriptions[30]="Move the cursor to the corner of the active window"
    descriptions[31]="Move a selected window"
    descriptions[33]="Move the active window in a direction or to a monitor"
    descriptions[34]="Lists all global shortcuts"
    descriptions[35]="List all windows with their properties"
    descriptions[37]="Temporarily enable or disable binds:ignore_group_lock"
    descriptions[38]="Print the current random splash"
    descriptions[39]="Execute a raw shell command"
    descriptions[40]="List active outputs with their properties"
    descriptions[43]="Disable output"
    descriptions[44]="Gets the current config info about animations and beziers"
    descriptions[47]="Change the split ratio"
    descriptions[48]="Move the active window"
    descriptions[49]="Pass the key to a specified window"
    descriptions[50]="Swap the focused window with the next window"
    descriptions[51]="List all connected keyboards and mice"
    descriptions[52]="List the layers"
    descriptions[54]="Lock the focused group"
    descriptions[55]="OK"
    descriptions[56]="Move a workspace to a monitor"
    descriptions[58]="Specify the Hyprland instance"
    descriptions[59]="Disable output"
    descriptions[61]="Pin a window"
    descriptions[62]="WARNING"
    descriptions[63]="INFO"
    descriptions[66]="Set the current window's floating state to true"
    descriptions[69]="On shortcut X sends shortcut Y to a specified window"
    descriptions[70]="List all workspaces with their properties"
    descriptions[71]="Focus the next window on a workspace"
    descriptions[72]="Modify the window stack order of the active or specified window"
    descriptions[73]="Toggle the current active window into a group"
    descriptions[74]="Lock the groups"
    descriptions[76]="Set all monitors' DPMS status"
    descriptions[77]="Switch focus from current to previously focused window"
    descriptions[78]="No Icon"
    descriptions[79]="Execute a batch of commands separated by ;"
    descriptions[80]="Send a notification using the built-in Hyprland notification system"
    descriptions[82]="List all running Hyprland instances and their info"
    descriptions[83]="Maximize no fullscreen"
    descriptions[84]="Maximize and fullscreen"
    descriptions[85]="Move the active window out of a group"
    descriptions[86]="Close the active window"
    descriptions[87]="HINT"
    descriptions[88]="Move the focused window to a workspace"
    descriptions[89]="Move the cursor to a specified position"
    descriptions[90]="List all current config parsing errors"
    descriptions[91]="Close a specified window"
    descriptions[92]="Swap the active window with another window"
    descriptions[93]="Apply a tag to the window"
    descriptions[94]="Force the renderer to reload all resources and outputs"
    descriptions[95]="Center the active window"
    descriptions[97]="Focus the first window matching"
    descriptions[98]="Set the hyprctl error string"
    descriptions[101]="List all registered binds"
    descriptions[102]="Print the Hyprland version: flags, commit and branch of build"
    descriptions[103]="Prints the help message"
    descriptions[104]="Toggle a special workspace on/off"
    descriptions[105]="Toggle the focused window's fullscreen state"
    descriptions[107]="None"
    descriptions[108]="Issue a keyword to call a config keyword dynamically"
    descriptions[109]="Toggle the current window to always be opaque"
    descriptions[110]="ERROR"
    descriptions[111]="Specify the Hyprland instance"
    descriptions[112]="Toggle the current window's floating state"
    descriptions[113]="Rename a workspace"
    descriptions[115]="Get the active workspace name and its properties"
    descriptions[117]="Get into a kill mode, where you can kill an app by clicking on it"
    descriptions[119]="Allows adding/removing fake outputs to a specific backend"
    descriptions[120]="Execute a Global Shortcut using the GlobalShortcuts portal"
    descriptions[121]="Issue a dispatch to call a keybind dispatcher with an arg"
    descriptions[122]="Force reload the config"
    descriptions[124]="Output in JSON format"
    descriptions[125]="Emits a custom event to socket2"
    descriptions[126]="Prints the help message"
    descriptions[128]="Current"
    descriptions[129]="Get the active window name and its properties"
    descriptions[131]="Dismiss all or up to amount of notifications"
    descriptions[132]="Focus a monitor"
    descriptions[133]="Move the focus in a direction"
    descriptions[134]="Interact with a plugin"
    descriptions[135]="Exit the compositor with no questions asked"
    descriptions[136]="Change the workspace"
    descriptions[137]="Sets the focused window’s fullscreen mode and the one sent to the client"
    descriptions[138]="Get the config option status (values)"
    descriptions[141]="List all decorations and their info"
    descriptions[142]="Set the current window's floating state to false"
    descriptions[144]="Return a parsable JSON with all the config options, descriptions, value types and ranges"
    descriptions[145]="Resize a selected window"
    descriptions[146]="Toggle the focused window's internal fullscreen state"
    descriptions[147]="Print tail of the log"
    descriptions[148]="Swap the active workspaces between two monitors"
    descriptions[149]="Change the current mapping group"
    descriptions[151]="Behave as moveintogroup"
    descriptions[152]="Get the current cursor pos in global layout coordinates"
    descriptions[154]="Focus the requested workspace"

    local -A literal_transitions
    literal_transitions[1]="([121]=15 [44]=3 [126]=22 [82]=3 [4]=22 [52]=3 [51]=3 [129]=3 [90]=3 [59]=22 [9]=3 [11]=3 [12]=4 [131]=5 [14]=6 [98]=7 [102]=3 [103]=22 [134]=8 [101]=3 [138]=3 [23]=3 [20]=3 [141]=9 [26]=3 [144]=3 [108]=10 [147]=11 [70]=3 [34]=3 [35]=3 [79]=22 [115]=3 [38]=3 [152]=3 [117]=3 [122]=14 [124]=22 [40]=12 [43]=22 [80]=16 [119]=13)"
    literal_transitions[2]="([82]=3 [52]=3 [51]=3 [129]=3 [9]=3 [90]=3 [11]=3 [12]=4 [131]=5 [14]=6 [98]=7 [102]=3 [134]=8 [101]=3 [23]=3 [20]=3 [138]=3 [141]=9 [26]=3 [144]=3 [108]=10 [147]=11 [70]=3 [34]=3 [35]=3 [115]=3 [38]=3 [152]=3 [117]=3 [40]=12 [119]=13 [122]=14 [121]=15 [80]=16 [44]=3)"
    literal_transitions[4]="([140]=3 [64]=17 [65]=17 [46]=17 [106]=17 [28]=3 [27]=3 [53]=5 [6]=17 [67]=3 [68]=17 [130]=17 [114]=17 [13]=3 [75]=5 [100]=3 [36]=17 [153]=17 [99]=17 [60]=17 [118]=17 [42]=17 [18]=3 [139]=17 [155]=3 [123]=17)"
    literal_transitions[7]="([127]=3)"
    literal_transitions[11]="([57]=3)"
    literal_transitions[12]="([10]=3)"
    literal_transitions[13]="([15]=20 [81]=23)"
    literal_transitions[14]="([143]=3)"
    literal_transitions[15]="([1]=3 [85]=3 [3]=3 [86]=3 [5]=3 [88]=3 [89]=3 [91]=3 [92]=3 [93]=3 [94]=3 [95]=3 [97]=3 [16]=3 [19]=3 [104]=3 [22]=3 [105]=3 [24]=3 [25]=3 [29]=3 [30]=3 [31]=3 [109]=3 [112]=3 [33]=3 [113]=3 [37]=3 [39]=3 [120]=3 [125]=3 [47]=3 [48]=3 [49]=3 [50]=3 [54]=3 [56]=3 [132]=3 [133]=3 [135]=3 [136]=3 [61]=3 [137]=21 [142]=3 [66]=3 [145]=3 [146]=3 [69]=3 [148]=3 [71]=3 [72]=3 [73]=3 [74]=3 [149]=3 [76]=3 [77]=3 [151]=3 [154]=3)"
    literal_transitions[16]="([87]=5 [7]=5 [110]=5 [62]=5 [78]=5 [55]=5 [63]=5)"
    literal_transitions[17]="([41]=3 [45]=3)"
    literal_transitions[18]="([8]=24)"
    literal_transitions[19]="([32]=3 [150]=3)"
    literal_transitions[20]="([96]=3 [17]=3 [116]=3 [21]=3)"
    literal_transitions[21]="([107]=3 [83]=3 [128]=3 [2]=3 [84]=3)"
    literal_transitions[24]="([58]=22 [111]=22)"

    local -A match_anything_transitions
    match_anything_transitions=([7]=18 [8]=3 [1]=2 [23]=3 [6]=19 [5]=3 [3]=18 [19]=3 [12]=18 [9]=3 [10]=3 [14]=18 [11]=18 [2]=2)

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
    local -A commands=([8]=0 [23]=1 [9]=3 [6]=2)

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
