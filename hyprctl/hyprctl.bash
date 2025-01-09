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
    if [[ $(type -t _get_comp_words_by_ref) != function ]]; then
        echo _get_comp_words_by_ref: function not defined.  Make sure the bash-completions system package is installed
        return 1
    fi

    local words cword
    _get_comp_words_by_ref -n "$COMP_WORDBREAKS" words cword

    declare -a literals=(resizeactive 2 changegroupactive -r moveintogroup forceallowsinput 4 ::= systeminfo all layouts setprop animationstyle switchxkblayout create denywindowfromgroup headless activebordercolor exec setcursor wayland focusurgentorlast workspacerules movecurrentworkspacetomonitor movetoworkspacesilent hyprpaper alpha inactivebordercolor movegroupwindow movecursortocorner movewindowpixel prev movewindow globalshortcuts clients dimaround setignoregrouplock splash execr monitors 0 forcenoborder -q animations 1 nomaxsize splitratio moveactive pass swapnext devices layers rounding lockactivegroup 5 moveworkspacetomonitor -f -i --quiet forcenodim pin 0 1 forceopaque forcenoshadow setfloating minsize alphaoverride sendshortcut workspaces cyclenext alterzorder togglegroup lockgroups bordersize dpms focuscurrentorlast -1 --batch notify remove instances 1 3 moveoutofgroup killactive 2 movetoworkspace movecursor configerrors closewindow swapwindow tagwindow forcerendererreload centerwindow auto focuswindow seterror nofocus alphafullscreen binds version -h togglespecialworkspace fullscreen windowdancecompat 0 keyword toggleopaque 3 --instance togglefloating renameworkspace alphafullscreenoverride activeworkspace x11 kill forceopaqueoverriden output global dispatch reload forcenoblur -j event --help disable -1 activewindow keepaspectratio dismissnotify focusmonitor movefocus plugin exit workspace fullscreenstate getoption alphainactiveoverride alphainactive decorations settiled config-only descriptions resizewindowpixel fakefullscreen rollinglog swapactiveworkspaces submap next movewindoworgroup cursorpos forcenoanims focusworkspaceoncurrentmonitor maxsize movecursortomonitor)
    declare -A literal_transitions
    literal_transitions[0]="([120]=14 [43]=2 [125]=21 [81]=2 [3]=21 [51]=2 [50]=2 [128]=2 [89]=2 [58]=21 [8]=2 [10]=2 [11]=3 [130]=4 [13]=5 [97]=6 [101]=2 [102]=21 [133]=7 [100]=2 [137]=2 [22]=2 [19]=2 [140]=8 [25]=2 [143]=2 [107]=9 [146]=10 [69]=2 [33]=2 [34]=2 [78]=21 [114]=2 [37]=2 [151]=2 [116]=2 [121]=13 [123]=21 [39]=11 [42]=21 [79]=15 [118]=12)"
    literal_transitions[1]="([81]=2 [51]=2 [50]=2 [128]=2 [8]=2 [89]=2 [10]=2 [11]=3 [130]=4 [13]=5 [97]=6 [101]=2 [133]=7 [100]=2 [22]=2 [19]=2 [137]=2 [140]=8 [25]=2 [143]=2 [107]=9 [146]=10 [69]=2 [33]=2 [34]=2 [114]=2 [37]=2 [151]=2 [116]=2 [39]=11 [118]=12 [121]=13 [120]=14 [79]=15 [43]=2)"
    literal_transitions[3]="([139]=2 [63]=16 [64]=16 [45]=16 [105]=16 [27]=2 [26]=2 [52]=4 [5]=16 [66]=2 [67]=16 [129]=16 [113]=16 [12]=2 [74]=4 [99]=2 [35]=16 [152]=16 [98]=16 [59]=16 [117]=16 [41]=16 [17]=2 [138]=16 [154]=2 [155]=2 [122]=16)"
    literal_transitions[6]="([126]=2)"
    literal_transitions[10]="([56]=2)"
    literal_transitions[11]="([9]=2)"
    literal_transitions[12]="([14]=19 [80]=22)"
    literal_transitions[13]="([142]=2)"
    literal_transitions[14]="([0]=2 [84]=2 [2]=2 [85]=2 [4]=2 [87]=2 [88]=2 [90]=2 [91]=2 [92]=2 [93]=2 [94]=2 [96]=2 [15]=2 [18]=2 [103]=2 [21]=2 [104]=2 [23]=2 [24]=2 [28]=2 [29]=2 [30]=2 [108]=2 [111]=2 [32]=2 [112]=2 [36]=2 [38]=2 [119]=2 [124]=2 [46]=2 [47]=2 [48]=2 [49]=2 [53]=2 [55]=2 [131]=2 [132]=2 [134]=2 [135]=2 [60]=2 [136]=20 [141]=2 [65]=2 [144]=2 [145]=2 [68]=2 [147]=2 [70]=2 [71]=2 [72]=2 [73]=2 [148]=2 [75]=2 [76]=2 [150]=2 [153]=2)"
    literal_transitions[15]="([86]=4 [6]=4 [109]=4 [61]=4 [77]=4 [54]=4 [62]=4)"
    literal_transitions[16]="([40]=2 [44]=2)"
    literal_transitions[17]="([7]=23)"
    literal_transitions[18]="([31]=2 [149]=2)"
    literal_transitions[19]="([95]=2 [16]=2 [115]=2 [20]=2)"
    literal_transitions[20]="([106]=2 [82]=2 [127]=2 [1]=2 [83]=2)"
    literal_transitions[23]="([57]=21 [110]=21)"
    declare -A match_anything_transitions=([6]=17 [7]=2 [0]=1 [22]=2 [5]=18 [4]=2 [2]=17 [18]=2 [11]=17 [8]=2 [9]=2 [13]=17 [10]=17 [1]=1)
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


    local -a matches=()

    local prefix="${words[$cword]}"
    if [[ -v "literal_transitions[$state]" ]]; then
        local state_transitions_initializer=${literal_transitions[$state]}
        declare -A state_transitions
        eval "state_transitions=$state_transitions_initializer"

        for literal_id in "${!state_transitions[@]}"; do
            local literal="${literals[$literal_id]}"
            if [[ $literal = "${prefix}"* ]]; then
                matches+=("$literal ")
            fi
        done
    fi
    declare -A commands
    commands=([7]=0 [22]=1 [8]=3 [5]=2)
    if [[ -v "commands[$state]" ]]; then
        local command_id=${commands[$state]}
        local completions=()
        readarray -t completions < <(_hyprctl_cmd_${command_id} "$prefix" | cut -f1)
        for item in "${completions[@]}"; do
            if [[ $item = "${prefix}"* ]]; then
                matches+=("$item")
            fi
        done
    fi


    local shortest_suffix="$prefix"
    for ((i=0; i < ${#COMP_WORDBREAKS}; i++)); do
        local char="${COMP_WORDBREAKS:$i:1}"
        local candidate=${prefix##*$char}
        if [[ ${#candidate} -lt ${#shortest_suffix} ]]; then
            shortest_suffix=$candidate
        fi
    done
    local superfluous_prefix=""
    if [[ "$shortest_suffix" != "$prefix" ]]; then
        local superfluous_prefix=${prefix%$shortest_suffix}
    fi
    COMPREPLY=("${matches[@]#$superfluous_prefix}")

    return 0
}

complete -o nospace -F _hyprctl hyprctl
