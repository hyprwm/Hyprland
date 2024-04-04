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

    local -a literals=("cyclenext" "globalshortcuts" "cursorpos" "bordersize" "renameworkspace" "animationstyle" "focuswindow" "0" "auto" "swapnext" "forceallowsinput" "moveactive" "activebordercolor" "wayland" "layers" "minsize" "monitors" "1" "3" "settiled" "kill" "focusmonitor" "swapwindow" "moveoutofgroup" "notify" "movecursor" "setcursor" "seterror" "4" "movecurrentworkspacetomonitor" "nomaxsize" "forcenoanims" "setprop" "-i" "togglefloating" "workspacerules" "movetoworkspace" "disable" "setignoregrouplock" "workspaces" "0" "closewindow" "movegroupwindow" "binds" "movewindow" "splitratio" "alpha" "denywindowfromgroup" "workspace" "configerrors" "togglegroup" "getoption" "forceopaque" "keepaspectratio" "--instance" "killactive" "pass" "decorations" "devices" "focuscurrentorlast" "submap" "global" "headless" "forcerendererreload" "movewindowpixel" "version" "dpms" "resizeactive" "moveintogroup" "5" "alphaoverride" "setfloating" "rollinglog" "::=" "rounding" "layouts" "moveworkspacetomonitor" "exec" "alphainactiveoverride" "alterzorder" "fakefullscreen" "nofocus" "keyword" "forcenoborder" "forcenodim" "pin" "output" "forcenoblur" "togglespecialworkspace" "fullscreen" "toggleopaque" "focusworkspaceoncurrentmonitor" "next" "changegroupactive" "-j" "instances" "execr" "exit" "clients" "all" "--batch" "dismissnotify" "inactivebordercolor" "switchxkblayout" "movetoworkspacesilent" "movewindoworgroup" "-r" "movefocus" "focusurgentorlast" "remove" "activeworkspace" "dispatch" "create" "centerwindow" "2" "hyprpaper" "-1" "reload" "alphainactive" "systeminfo" "plugin" "dimaround" "activewindow" "swapactiveworkspaces" "splash" "maxsize" "lockactivegroup" "windowdancecompat" "forceopaqueoverriden" "lockgroups" "movecursortocorner" "x11" "prev" "1" "resizewindowpixel" "forcenoshadow")

    declare -A literal_transitions
    literal_transitions[0]="([101]=1 [72]=2 [32]=3 [1]=2 [2]=2 [75]=2 [103]=4 [35]=2 [106]=5 [39]=2 [43]=2 [110]=2 [82]=6 [111]=8 [49]=2 [51]=2 [86]=9 [115]=2 [117]=2 [119]=2 [14]=2 [57]=10 [58]=2 [16]=11 [120]=12 [20]=2 [122]=2 [124]=2 [24]=13 [65]=2 [94]=5 [95]=2 [26]=2 [27]=14 [98]=2 [100]=5)"
    literal_transitions[3]="([70]=26 [31]=26 [52]=26 [53]=26 [87]=26 [102]=2 [118]=2 [74]=1 [15]=2 [121]=26 [3]=1 [5]=2 [125]=2 [127]=26 [78]=26 [128]=26 [81]=26 [30]=26 [46]=2 [12]=2 [83]=26 [10]=26 [84]=26 [135]=26)"
    literal_transitions[7]="([101]=1 [72]=2 [32]=3 [1]=2 [2]=2 [75]=2 [103]=4 [35]=2 [39]=2 [43]=2 [110]=2 [82]=6 [111]=8 [49]=2 [51]=2 [86]=9 [115]=2 [117]=2 [119]=2 [14]=2 [57]=10 [58]=2 [16]=11 [120]=12 [20]=2 [122]=2 [124]=2 [24]=13 [65]=2 [95]=2 [26]=2 [27]=14 [98]=2)"
    literal_transitions[8]="([126]=2 [129]=2 [0]=2 [71]=2 [34]=2 [104]=2 [36]=2 [105]=2 [4]=2 [76]=2 [38]=2 [77]=2 [108]=2 [6]=2 [42]=2 [41]=2 [79]=2 [80]=2 [44]=2 [45]=2 [9]=2 [107]=2 [48]=2 [50]=2 [11]=2 [113]=2 [85]=2 [47]=2 [55]=2 [88]=2 [56]=2 [89]=2 [90]=2 [59]=2 [60]=2 [123]=2 [91]=2 [61]=2 [19]=2 [93]=2 [21]=2 [22]=2 [63]=2 [64]=2 [23]=2 [130]=2 [25]=2 [66]=2 [96]=2 [67]=2 [29]=2 [134]=2 [68]=2 [97]=2)"
    literal_transitions[9]="([112]=17 [109]=20)"
    literal_transitions[10]="([18]=2 [114]=2 [28]=2 [133]=2 [69]=2 [116]=2)"
    literal_transitions[11]="([99]=2)"
    literal_transitions[14]="([37]=2)"
    literal_transitions[15]="([73]=16)"
    literal_transitions[17]="([8]=2 [62]=2 [13]=2 [131]=2)"
    literal_transitions[18]="([73]=19)"
    literal_transitions[22]="([73]=23)"
    literal_transitions[23]="([40]=2)"
    literal_transitions[24]="([33]=5 [54]=5)"
    literal_transitions[25]="([73]=24)"
    literal_transitions[26]="([17]=2 [7]=2)"
    literal_transitions[27]="([132]=2 [92]=2)"
    literal_transitions[29]="([73]=32)"

    declare -A match_anything_transitions
    match_anything_transitions=([1]=2 [27]=2 [10]=31 [30]=22 [14]=25 [7]=7 [2]=25 [28]=29 [16]=21 [12]=2 [31]=15 [0]=7 [19]=28 [20]=2 [6]=2 [32]=30 [13]=1 [11]=25 [21]=18 [4]=27)
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
    commands=([32]=3 [16]=1 [19]=2 [10]=0)
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
