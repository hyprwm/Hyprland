_hyprctl_cmd_2 () {
    hyprctl monitors | awk '/Monitor/{ print $2 }'
}

_hyprctl_cmd_3 () {
    hyprpm list | awk '/Plugin/{ print $4 }'
}

_hyprctl_cmd_0 () {
    hyprctl clients | awk '/class/{ print $2 }'
}

_hyprctl_cmd_1 () {
    hyprctl devices | sed -n '/Keyboard at/{n; s/^\s\+//; p}'
}

_hyprctl () {
    if [[ $(type -t _get_comp_words_by_ref) != function ]]; then
        echo _get_comp_words_by_ref: function not defined.  Make sure the bash-completions system package is installed
        return 1
    fi

    local words cword
    _get_comp_words_by_ref -n "$COMP_WORDBREAKS" words cword

    local -a literals=("cyclenext" "globalshortcuts" "cursorpos" "bordersize" "renameworkspace" "animationstyle" "focuswindow" "0" "auto" "swapnext" "forceallowsinput" "moveactive" "activebordercolor" "alphafullscreen" "wayland" "layers" "minsize" "monitors" "1" "kill" "settiled" "3" "focusmonitor" "swapwindow" "moveoutofgroup" "notify" "movecursor" "setcursor" "seterror" "movecurrentworkspacetomonitor" "4" "nomaxsize" "forcenoanims" "setprop" "-i" "-q" "togglefloating" "workspacerules" "movetoworkspace" "disable" "setignoregrouplock" "workspaces" "movegroupwindow" "closewindow" "0" "--instance" "binds" "movewindow" "splitratio" "alpha" "denywindowfromgroup" "workspace" "configerrors" "togglegroup" "getoption" "forceopaque" "keepaspectratio" "killactive" "pass" "decorations" "devices" "focuscurrentorlast" "submap" "global" "alphafullscreenoverride" "forcerendererreload" "movewindowpixel" "headless" "version" "dpms" "resizeactive" "moveintogroup" "5" "alphaoverride" "setfloating" "rollinglog" "::=" "rounding" "layouts" "moveworkspacetomonitor" "exec" "alphainactiveoverride" "alterzorder" "fakefullscreen" "nofocus" "keyword" "forcenoborder" "forcenodim" "--quiet" "pin" "output" "forcenoblur" "togglespecialworkspace" "fullscreen" "toggleopaque" "focusworkspaceoncurrentmonitor" "next" "changegroupactive" "-j" "instances" "execr" "exit" "clients" "all" "--batch" "dismissnotify" "inactivebordercolor" "switchxkblayout" "movetoworkspacesilent" "tagwindow" "movewindoworgroup" "-r" "movefocus" "focusurgentorlast" "remove" "activeworkspace" "dispatch" "create" "centerwindow" "2" "hyprpaper" "-1" "reload" "alphainactive" "systeminfo" "plugin" "dimaround" "activewindow" "swapactiveworkspaces" "splash" "sendshortcut" "maxsize" "lockactivegroup" "windowdancecompat" "forceopaqueoverriden" "lockgroups" "movecursortocorner" "x11" "prev" "1" "resizewindowpixel" "forcenoshadow")

    declare -A literal_transitions
    literal_transitions[0]="([105]=1 [75]=2 [33]=3 [35]=4 [1]=2 [2]=2 [78]=2 [107]=5 [37]=2 [111]=4 [41]=2 [46]=2 [115]=2 [85]=6 [116]=8 [52]=2 [88]=4 [54]=2 [90]=9 [120]=2 [122]=2 [124]=2 [15]=2 [59]=10 [60]=2 [17]=11 [125]=12 [19]=2 [127]=2 [129]=2 [25]=13 [68]=2 [98]=4 [99]=2 [27]=2 [28]=14 [102]=2 [104]=4)"
    literal_transitions[3]="([73]=17 [13]=2 [32]=17 [55]=17 [56]=17 [91]=17 [106]=2 [123]=2 [77]=1 [16]=2 [126]=17 [3]=1 [5]=2 [64]=17 [131]=2 [133]=17 [81]=17 [134]=17 [84]=17 [31]=17 [49]=2 [12]=2 [86]=17 [10]=17 [87]=17 [141]=17)"
    literal_transitions[7]="([105]=1 [75]=2 [33]=3 [1]=2 [2]=2 [78]=2 [107]=5 [37]=2 [41]=2 [46]=2 [115]=2 [85]=6 [116]=8 [52]=2 [54]=2 [90]=9 [120]=2 [122]=2 [124]=2 [15]=2 [59]=10 [60]=2 [17]=11 [125]=12 [19]=2 [127]=2 [129]=2 [25]=13 [68]=2 [99]=2 [27]=2 [28]=14 [102]=2)"
    literal_transitions[8]="([101]=2 [130]=2 [132]=2 [0]=2 [74]=2 [36]=2 [108]=2 [109]=2 [38]=2 [110]=2 [4]=2 [79]=2 [40]=2 [80]=2 [113]=2 [6]=2 [42]=2 [43]=2 [82]=2 [83]=2 [47]=2 [48]=2 [9]=2 [50]=2 [51]=2 [53]=2 [11]=2 [112]=2 [89]=2 [118]=2 [57]=2 [92]=2 [58]=2 [93]=2 [94]=2 [61]=2 [62]=2 [128]=2 [95]=2 [63]=2 [20]=2 [97]=2 [22]=2 [23]=2 [65]=2 [66]=2 [135]=2 [136]=2 [24]=2 [26]=2 [69]=2 [100]=2 [70]=2 [140]=2 [29]=2 [71]=2)"
    literal_transitions[9]="([117]=20 [114]=16)"
    literal_transitions[11]="([103]=2)"
    literal_transitions[13]="([21]=1 [119]=1 [30]=1 [139]=1 [121]=1 [44]=1 [72]=1)"
    literal_transitions[14]="([39]=2)"
    literal_transitions[15]="([138]=2 [96]=2)"
    literal_transitions[17]="([18]=2 [7]=2)"
    literal_transitions[18]="([76]=19)"
    literal_transitions[19]="([34]=4 [45]=4)"
    literal_transitions[20]="([8]=2 [67]=2 [14]=2 [137]=2)"

    declare -A match_anything_transitions
    match_anything_transitions=([1]=2 [0]=7 [6]=2 [15]=2 [10]=2 [5]=15 [14]=18 [7]=7 [2]=18 [16]=2 [12]=2 [11]=18)
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
    commands=([5]=1 [16]=2 [12]=3 [10]=0)
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
