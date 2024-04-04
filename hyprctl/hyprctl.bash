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
    if [[ $(type -t _get_comp_words_by_ref) != function ]]; then
        echo _get_comp_words_by_ref: function not defined.  Make sure the bash-completions system package is installed
        return 1
    fi

    local words cword
    _get_comp_words_by_ref -n "$COMP_WORDBREAKS" words cword

    local -a literals=("focusmonitor" "exit" "global" "forceallowsinput" "::=" "movecursortocorner" "movewindowpixel" "activeworkspace" "monitors" "movecurrentworkspacetomonitor" "togglespecialworkspace" "all" "animationstyle" "closewindow" "setprop" "clients" "denywindowfromgroup" "create" "moveoutofgroup" "headless" "activebordercolor" "rollinglog" "wayland" "movewindoworgroup" "setcursor" "fakefullscreen" "moveactive" "prev" "hyprpaper" "alpha" "inactivebordercolor" "-i" "--instance" "togglefloating" "settiled" "swapwindow" "dimaround" "setignoregrouplock" "layouts" "0" "forcenoborder" "notify" "binds" "focuswindow" "seterror" "1" "systeminfo" "exec" "cyclenext" "nomaxsize" "reload" "rounding" "layers" "setfloating" "5" "lockactivegroup" "movetoworkspace" "swapactiveworkspaces" "changegroupactive" "forcenodim" "0" "configerrors" "4" "forceopaque" "forcenoshadow" "workspaces" "1" "swapnext" "minsize" "alphaoverride" "toggleopaque" "decorations" "alterzorder" "bordersize" "-1" "focuscurrentorlast" "workspacerules" "splitratio" "remove" "renameworkspace" "movetoworkspacesilent" "killactive" "pass" "getoption" "switchxkblayout" "2" "auto" "pin" "version" "nofocus" "togglegroup" "workspace" "lockgroups" "-r" "movewindow" "cursorpos" "focusworkspaceoncurrentmonitor" "execr" "windowdancecompat" "globalshortcuts" "3" "keyword" "movefocus" "movecursor" "instances" "dpms" "x11" "moveintogroup" "resizewindowpixel" "kill" "moveworkspacetomonitor" "forceopaqueoverriden" "dispatch" "-j" "forcenoblur" "devices" "disable" "-b" "activewindow" "fullscreen" "keepaspectratio" "output" "plugin" "alphainactiveoverride" "alphainactive" "resizeactive" "centerwindow" "splash" "focusurgentorlast" "submap" "next" "movegroupwindow" "forcenoanims" "forcerendererreload" "maxsize" "dismissnotify")

    declare -A literal_transitions
    literal_transitions[0]="([44]=31 [115]=3 [46]=3 [84]=21 [83]=3 [117]=11 [50]=3 [52]=3 [118]=3 [7]=3 [8]=28 [121]=25 [14]=7 [88]=3 [15]=3 [122]=24 [61]=3 [93]=11 [21]=3 [24]=3 [65]=3 [95]=3 [28]=3 [99]=3 [127]=3 [101]=30 [71]=18 [104]=3 [38]=3 [109]=3 [76]=3 [113]=11 [112]=27 [41]=1 [42]=3 [135]=2)"
    literal_transitions[6]="([86]=3 [19]=3 [106]=3 [22]=3)"
    literal_transitions[7]="([124]=3 [63]=8 [64]=8 [49]=8 [98]=8 [30]=3 [29]=3 [51]=2 [3]=8 [68]=3 [69]=8 [120]=8 [12]=3 [73]=2 [36]=8 [132]=8 [89]=8 [59]=8 [111]=8 [40]=8 [20]=3 [123]=8 [134]=3 [114]=8)"
    literal_transitions[8]="([39]=3 [45]=3)"
    literal_transitions[9]="([4]=10)"
    literal_transitions[10]="([32]=11 [31]=11)"
    literal_transitions[12]="([27]=3 [130]=3)"
    literal_transitions[15]="([4]=4)"
    literal_transitions[16]="([4]=17)"
    literal_transitions[17]="([60]=3)"
    literal_transitions[18]="([62]=3 [85]=3 [100]=3 [66]=3 [74]=3 [54]=3)"
    literal_transitions[20]="([4]=13)"
    literal_transitions[25]="([17]=6 [78]=26)"
    literal_transitions[27]="([0]=3 [1]=3 [47]=3 [48]=3 [2]=3 [80]=3 [81]=3 [82]=3 [5]=3 [6]=3 [53]=3 [55]=3 [10]=3 [9]=3 [56]=3 [119]=3 [13]=3 [87]=3 [16]=3 [57]=3 [18]=3 [58]=3 [90]=3 [94]=3 [96]=3 [91]=3 [23]=3 [92]=3 [25]=3 [26]=3 [67]=3 [97]=3 [125]=3 [126]=3 [33]=3 [70]=3 [102]=3 [34]=3 [103]=3 [72]=3 [35]=3 [128]=3 [129]=3 [75]=3 [37]=3 [105]=3 [107]=3 [108]=3 [77]=3 [110]=3 [131]=3 [43]=3 [133]=3 [79]=3)"
    literal_transitions[28]="([11]=3)"
    literal_transitions[29]="([42]=3 [115]=3 [44]=31 [50]=3 [83]=3 [84]=21 [52]=3 [118]=3 [7]=3 [8]=28 [121]=25 [14]=7 [88]=3 [15]=3 [122]=24 [61]=3 [21]=3 [24]=3 [65]=3 [95]=3 [28]=3 [99]=3 [127]=3 [101]=30 [71]=18 [104]=3 [38]=3 [109]=3 [76]=3 [112]=27 [41]=1 [46]=3 [135]=2)"
    literal_transitions[31]="([116]=3)"
    literal_transitions[32]="([4]=22)"

    declare -A match_anything_transitions
    match_anything_transitions=([0]=29 [4]=5 [23]=15 [2]=3 [18]=19 [28]=9 [31]=9 [24]=3 [29]=29 [13]=14 [14]=32 [1]=2 [22]=23 [19]=20 [5]=16 [12]=3 [30]=3 [3]=9 [21]=12 [26]=3)
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
    commands=([4]=0 [13]=2 [22]=3 [18]=1)
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
