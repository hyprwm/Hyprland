#compdef hyprpm

_hyprpm_cmd_0 () {
    hyprpm list | awk '/Plugin/{print $4}'
}

_hyprpm_cmd_1 () {
    hyprpm list | awk '/Repository/{print $4}' | sed 's/:$//'
}

_hyprpm () {
    local -a literals=("--no-shallow" "-n" "::=" "disable" "list" "--help" "update" "add" "--verbose" "-v" "--force" "-s" "remove" "enable" "--notify" "-h" "reload" "-f")

    local -A descriptions
    descriptions[1]="Disable shallow cloning of Hyprland sources"
    descriptions[2]="Send a hyprland notification for important events (e.g. load fail)"
    descriptions[4]="Unload a plugin"
    descriptions[5]="List all installed plugins"
    descriptions[6]="Show help menu"
    descriptions[7]="Check and update all plugins if needed"
    descriptions[8]="Install a new plugin repository from git"
    descriptions[9]="Enable too much logging"
    descriptions[10]="Enable too much logging"
    descriptions[11]="Force an operation ignoring checks (e.g. update -f)"
    descriptions[12]="Disable shallow cloning of Hyprland sources"
    descriptions[13]="Remove a plugin repository"
    descriptions[14]="Load a plugin"
    descriptions[15]="Send a hyprland notification for important events (e.g. load fail)"
    descriptions[16]="Show help menu"
    descriptions[17]="Reload all plugins"
    descriptions[18]="Force an operation ignoring checks (e.g. update -f)"

    local -A literal_transitions
    literal_transitions[1]="([1]=8 [4]=4 [5]=5 [9]=8 [10]=8 [7]=5 [8]=5 [12]=8 [6]=8 [11]=8 [13]=3 [14]=4 [16]=8 [17]=5 [18]=8)"
    literal_transitions[2]="([13]=3 [14]=4 [4]=4 [5]=5 [17]=5 [7]=5 [8]=5)"
    literal_transitions[6]="([3]=7)"
    literal_transitions[7]="([2]=8 [15]=8)"

    local -A match_anything_transitions
    match_anything_transitions=([2]=2 [5]=6 [4]=5 [3]=5 [1]=2)

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
    local -A commands=([4]=0 [3]=1)

    if [[ -v "commands[$state]" ]]; then
        local command_id=${commands[$state]}
        local output=$(_hyprpm_cmd_${command_id} "${words[$CURRENT]}")
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
    compdef _hyprpm hyprpm
else
    _hyprpm
fi
