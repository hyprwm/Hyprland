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
    if [[ $(type -t _get_comp_words_by_ref) != function ]]; then
        echo _get_comp_words_by_ref: function not defined.  Make sure the bash-completions system package is installed
        return 1
    fi

    local words cword
    _get_comp_words_by_ref -n "$COMP_WORDBREAKS" words cword

    local -a literals=("resizeactive" "changegroupactive" "-r" "moveintogroup" "forceallowsinput" "4" "::=" "systeminfo" "all" "layouts" "animationstyle" "setprop" "switchxkblayout" "create" "denywindowfromgroup" "headless" "activebordercolor" "exec" "setcursor" "wayland" "focusurgentorlast" "workspacerules" "movecurrentworkspacetomonitor" "movetoworkspacesilent" "hyprpaper" "alpha" "inactivebordercolor" "movegroupwindow" "movecursortocorner" "movewindowpixel" "prev" "movewindow" "clients" "dimaround" "setignoregrouplock" "splash" "execr" "monitors" "0" "forcenoborder" "1" "nomaxsize" "splitratio" "moveactive" "pass" "swapnext" "devices" "layers" "rounding" "lockactivegroup" "5" "moveworkspacetomonitor" "-i" "forcenodim" "pin" "0" "1" "forceopaque" "forcenoshadow" "setfloating" "minsize" "alphaoverride" "workspaces" "cyclenext" "alterzorder" "togglegroup" "lockgroups" "bordersize" "dpms" "focuscurrentorlast" "-1" "--batch" "notify" "remove" "instances" "moveoutofgroup" "killactive" "2" "movetoworkspace" "movecursor" "configerrors" "closewindow" "swapwindow" "auto" "forcerendererreload" "centerwindow" "focuswindow" "seterror" "nofocus" "alphafullscreen" "binds" "version" "togglespecialworkspace" "fullscreen" "windowdancecompat" "globalshortcuts" "keyword" "toggleopaque" "3" "--instance" "togglefloating" "renameworkspace" "alphafullscreenoverride" "activeworkspace" "x11" "kill" "forceopaqueoverriden" "output" "global" "dispatch" "reload" "forcenoblur" "-j" "disable" "activewindow" "keepaspectratio" "dismissnotify" "focusmonitor" "movefocus" "plugin" "exit" "workspace" "getoption" "alphainactiveoverride" "alphainactive" "decorations" "settiled" "resizewindowpixel" "fakefullscreen" "rollinglog" "swapactiveworkspaces" "submap" "next" "movewindoworgroup" "cursorpos" "forcenoanims" "focusworkspaceoncurrentmonitor" "maxsize")

    declare -A literal_transitions
    literal_transitions[0]="([74]=2 [2]=17 [47]=2 [46]=2 [114]=2 [7]=2 [80]=2 [9]=2 [11]=3 [116]=4 [12]=5 [87]=6 [91]=2 [119]=7 [90]=2 [21]=2 [18]=2 [122]=2 [125]=8 [24]=2 [95]=2 [96]=9 [129]=2 [62]=2 [103]=2 [32]=2 [71]=17 [35]=2 [134]=2 [105]=2 [37]=10 [107]=11 [72]=13 [109]=12 [110]=2 [112]=17)"
    literal_transitions[1]="([47]=2 [46]=2 [114]=2 [7]=2 [80]=2 [9]=2 [11]=3 [116]=4 [12]=5 [87]=6 [91]=2 [119]=7 [90]=2 [21]=2 [18]=2 [122]=2 [125]=8 [24]=2 [95]=2 [96]=9 [129]=2 [62]=2 [103]=2 [32]=2 [35]=2 [105]=2 [37]=10 [107]=11 [110]=2 [109]=12 [72]=13 [134]=2 [74]=2)"
    literal_transitions[3]="([124]=2 [57]=14 [58]=14 [41]=14 [94]=14 [26]=2 [25]=2 [48]=4 [4]=14 [60]=2 [61]=14 [115]=14 [102]=14 [10]=2 [67]=4 [89]=2 [33]=14 [135]=14 [88]=14 [53]=14 [106]=14 [39]=14 [16]=2 [123]=14 [137]=2 [111]=14)"
    literal_transitions[6]="([113]=2)"
    literal_transitions[10]="([8]=2)"
    literal_transitions[11]="([13]=19 [73]=18)"
    literal_transitions[12]="([0]=2 [75]=2 [1]=2 [76]=2 [42]=2 [43]=2 [3]=2 [44]=2 [45]=2 [78]=2 [79]=2 [49]=2 [81]=2 [51]=2 [82]=2 [84]=2 [85]=2 [117]=2 [86]=2 [118]=2 [14]=2 [120]=2 [54]=2 [121]=2 [17]=2 [92]=2 [20]=2 [93]=2 [22]=2 [126]=2 [23]=2 [59]=2 [127]=2 [27]=2 [28]=2 [29]=2 [97]=2 [128]=2 [130]=2 [63]=2 [31]=2 [64]=2 [65]=2 [66]=2 [100]=2 [68]=2 [34]=2 [69]=2 [36]=2 [101]=2 [131]=2 [108]=2 [133]=2 [136]=2)"
    literal_transitions[13]="([77]=4 [5]=4 [98]=4 [55]=4 [70]=4 [50]=4 [56]=4)"
    literal_transitions[14]="([38]=2 [40]=2)"
    literal_transitions[15]="([6]=16)"
    literal_transitions[16]="([52]=17 [99]=17)"
    literal_transitions[19]="([83]=2 [15]=2 [104]=2 [19]=2)"
    literal_transitions[20]="([30]=2 [132]=2)"

    declare -A match_anything_transitions
    match_anything_transitions=([6]=15 [7]=2 [0]=1 [5]=20 [4]=2 [20]=2 [2]=15 [18]=2 [8]=2 [9]=2 [10]=15 [1]=1)
    declare -A subword_transitions

    local state=0
    local word_index=1
    while [[ $word_index -lt $cword ]]; do
        local word=${words[$word_index]}

        if [[ -v "literal_transitions[$state]" ]]; then
            declare -A state_transitions
            eval "state_transitions=${literal_transitions[$state]}"

            local word_matched=0
            for literal_id in $(seq 0 $((${#literals[@]} - 1))); do
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


    local prefix="${words[$cword]}"

    local shortest_suffix="$word"
    for ((i=0; i < ${#COMP_WORDBREAKS}; i++)); do
        local char="${COMP_WORDBREAKS:$i:1}"
        local candidate="${word##*$char}"
        if [[ ${#candidate} -lt ${#shortest_suffix} ]]; then
            shortest_suffix=$candidate
        fi
    done
    local superfluous_prefix=""
    if [[ "$shortest_suffix" != "$word" ]]; then
        local superfluous_prefix=${word%$shortest_suffix}
    fi

    if [[ -v "literal_transitions[$state]" ]]; then
        local state_transitions_initializer=${literal_transitions[$state]}
        declare -A state_transitions
        eval "state_transitions=$state_transitions_initializer"

        for literal_id in "${!state_transitions[@]}"; do
            local literal="${literals[$literal_id]}"
            if [[ $literal = "${prefix}"* ]]; then
                local completion=${literal#"$superfluous_prefix"}
                COMPREPLY+=("$completion ")
            fi
        done
    fi
    declare -A commands
    commands=([7]=0 [8]=3 [18]=2 [5]=1)
    if [[ -v "commands[$state]" ]]; then
        local command_id=${commands[$state]}
        local completions=()
        mapfile -t completions < <(_hyprctl_cmd_${command_id} "$prefix" | cut -f1)
        for item in "${completions[@]}"; do
            if [[ $item = "${prefix}"* ]]; then
                COMPREPLY+=("$item")
            fi
        done
    fi


    return 0
}

complete -o nospace -F _hyprctl hyprctl
