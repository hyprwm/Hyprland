_hyprpm_cmd_0 () {
    hyprpm list | awk '/Plugin/{print $4}'
}

_hyprpm_cmd_1 () {
    hyprpm list | awk '/Repository/{print $4}' | sed 's/:$//'
}

_hyprpm () {
    if [[ $(type -t _get_comp_words_by_ref) != function ]]; then
        echo _get_comp_words_by_ref: function not defined.  Make sure the bash-completions system package is installed
        return 1
    fi

    local words cword
    _get_comp_words_by_ref -n "$COMP_WORDBREAKS" words cword

    declare -a literals=(--no-shallow -n ::= disable list --help update add --verbose -v --force -s remove enable --notify -h reload -f)
    declare -A literal_transitions
    literal_transitions[0]="([0]=7 [3]=3 [4]=4 [8]=7 [9]=7 [6]=4 [7]=4 [11]=7 [5]=7 [10]=7 [12]=2 [13]=3 [15]=7 [16]=4 [17]=7)"
    literal_transitions[1]="([12]=2 [13]=3 [3]=3 [4]=4 [16]=4 [6]=4 [7]=4)"
    literal_transitions[5]="([2]=6)"
    literal_transitions[6]="([1]=7 [14]=7)"
    declare -A match_anything_transitions=([1]=1 [4]=5 [3]=4 [2]=4 [0]=1)
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
    commands=([3]=0 [2]=1)
    if [[ -v "commands[$state]" ]]; then
        local command_id=${commands[$state]}
        local completions=()
        readarray -t completions < <(_hyprpm_cmd_${command_id} "$prefix" | cut -f1)
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

complete -o nospace -F _hyprpm hyprpm
