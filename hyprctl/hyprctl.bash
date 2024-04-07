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
    if [[ $(type -t _get_comp_words_by_ref) != function ]]; then
        echo _get_comp_words_by_ref: function not defined.  Make sure the bash-completions system package is installed
        return 1
    fi

    local words cword
    _get_comp_words_by_ref -n "$COMP_WORDBREAKS" words cword

    local -a literals=("cyclenext" "globalshortcuts" "cursorpos" "bordersize" "renameworkspace" "animationstyle" "focuswindow" "0" "auto" "swapnext" "forceallowsinput" "moveactive" "activebordercolor" "alphafullscreen" "wayland" "layers" "minsize" "monitors" "1" "kill" "settiled" "3" "focusmonitor" "swapwindow" "moveoutofgroup" "notify" "movecursor" "setcursor" "seterror" "movecurrentworkspacetomonitor" "4" "nomaxsize" "forcenoanims" "setprop" "-i" "togglefloating" "workspacerules" "movetoworkspace" "disable" "setignoregrouplock" "workspaces" "movegroupwindow" "closewindow" "0" "--instance" "binds" "movewindow" "splitratio" "alpha" "denywindowfromgroup" "workspace" "configerrors" "togglegroup" "getoption" "forceopaque" "keepaspectratio" "killactive" "pass" "decorations" "devices" "focuscurrentorlast" "submap" "global" "alphafullscreenoverride" "forcerendererreload" "movewindowpixel" "headless" "version" "dpms" "resizeactive" "moveintogroup" "5" "alphaoverride" "setfloating" "rollinglog" "::=" "rounding" "layouts" "moveworkspacetomonitor" "exec" "alphainactiveoverride" "alterzorder" "fakefullscreen" "nofocus" "keyword" "forcenoborder" "forcenodim" "pin" "output" "forcenoblur" "togglespecialworkspace" "fullscreen" "toggleopaque" "focusworkspaceoncurrentmonitor" "next" "changegroupactive" "-j" "instances" "execr" "exit" "clients" "all" "--batch" "dismissnotify" "inactivebordercolor" "switchxkblayout" "movetoworkspacesilent" "movewindoworgroup" "-r" "movefocus" "focusurgentorlast" "remove" "activeworkspace" "dispatch" "create" "centerwindow" "2" "hyprpaper" "-1" "reload" "alphainactive" "systeminfo" "plugin" "dimaround" "activewindow" "swapactiveworkspaces" "splash" "maxsize" "lockactivegroup" "windowdancecompat" "forceopaqueoverriden" "lockgroups" "movecursortocorner" "x11" "prev" "1" "resizewindowpixel" "forcenoshadow")

    declare -A literal_transitions
    literal_transitions[0]="([103]=1 [74]=2 [33]=3 [1]=2 [2]=2 [77]=2 [105]=4 [36]=2 [108]=5 [40]=2 [45]=2 [112]=2 [84]=6 [113]=8 [51]=2 [53]=2 [88]=9 [117]=2 [119]=2 [121]=2 [15]=2 [58]=10 [59]=2 [17]=11 [122]=12 [19]=2 [124]=2 [126]=2 [25]=13 [67]=2 [96]=5 [97]=2 [27]=2 [28]=14 [100]=2 [102]=5)"
    literal_transitions[3]="([72]=18 [13]=2 [32]=18 [54]=18 [55]=18 [89]=18 [104]=2 [120]=2 [76]=1 [16]=2 [123]=18 [3]=1 [5]=2 [63]=18 [127]=2 [129]=18 [80]=18 [130]=18 [83]=18 [31]=18 [48]=2 [12]=2 [85]=18 [10]=18 [86]=18 [137]=18)"
    literal_transitions[7]="([103]=1 [74]=2 [33]=3 [1]=2 [2]=2 [77]=2 [105]=4 [36]=2 [40]=2 [45]=2 [112]=2 [84]=6 [113]=8 [51]=2 [53]=2 [88]=9 [117]=2 [119]=2 [121]=2 [15]=2 [58]=10 [59]=2 [17]=11 [122]=12 [19]=2 [124]=2 [126]=2 [25]=13 [67]=2 [97]=2 [27]=2 [28]=14 [100]=2)"
    literal_transitions[8]="([128]=2 [131]=2 [0]=2 [73]=2 [35]=2 [106]=2 [37]=2 [107]=2 [4]=2 [78]=2 [39]=2 [79]=2 [110]=2 [6]=2 [41]=2 [42]=2 [81]=2 [82]=2 [46]=2 [47]=2 [9]=2 [109]=2 [50]=2 [52]=2 [11]=2 [115]=2 [87]=2 [49]=2 [56]=2 [90]=2 [57]=2 [91]=2 [92]=2 [60]=2 [61]=2 [125]=2 [93]=2 [62]=2 [20]=2 [95]=2 [22]=2 [23]=2 [64]=2 [65]=2 [24]=2 [132]=2 [26]=2 [68]=2 [98]=2 [69]=2 [29]=2 [136]=2 [70]=2 [99]=2)"
    literal_transitions[9]="([114]=15 [111]=16)"
    literal_transitions[11]="([101]=2)"
    literal_transitions[13]="([21]=1 [116]=1 [30]=1 [135]=1 [118]=1 [43]=1 [71]=1)"
    literal_transitions[14]="([38]=2)"
    literal_transitions[15]="([8]=2 [66]=2 [14]=2 [133]=2)"
    literal_transitions[17]="([75]=19)"
    literal_transitions[18]="([18]=2 [7]=2)"
    literal_transitions[19]="([34]=5 [44]=5)"
    literal_transitions[20]="([134]=2 [94]=2)"

    declare -A match_anything_transitions
    match_anything_transitions=([1]=2 [0]=7 [6]=2 [20]=2 [10]=2 [2]=17 [7]=7 [12]=2 [14]=17 [16]=2 [4]=20 [11]=17)
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
    commands=([16]=2 [4]=3 [12]=1 [10]=0)
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
