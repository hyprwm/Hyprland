function _hyprpm_1
    set 1 $argv[1]
    hyprpm list | awk '/Plugin/{print $4}'
end

function _hyprpm
    set COMP_LINE (commandline --cut-at-cursor)

    set COMP_WORDS
    echo $COMP_LINE | read --tokenize --array COMP_WORDS
    if string match --quiet --regex '.*\s$' $COMP_LINE
        set COMP_CWORD (math (count $COMP_WORDS) + 1)
    else
        set COMP_CWORD (count $COMP_WORDS)
    end

    set --local literals "-n" "::=" "list" "disable" "--help" "update" "add" "--verbose" "-v" "--force" "remove" "enable" "--notify" "-h" "reload" "-f"

    set --local descriptions
    set descriptions[1] "Send a hyprland notification for important events (e.g. load fail)"
    set descriptions[3] "List all installed plugins"
    set descriptions[4] "Unload a plugin"
    set descriptions[5] "Show help menu"
    set descriptions[6] "Check and update all plugins if needed"
    set descriptions[7] "Install a new plugin repository from git"
    set descriptions[8] "Enable too much loggin"
    set descriptions[9] "Enable too much loggin"
    set descriptions[10] "Force an operation ignoring checks (e.g. update -f)"
    set descriptions[11] "Remove a plugin repository"
    set descriptions[12] "Load a plugin"
    set descriptions[13] "Send a hyprland notification for important events (e.g. load fail)"
    set descriptions[14] "Show help menu"
    set descriptions[15] "Reload all plugins"
    set descriptions[16] "Force an operation ignoring checks (e.g. update -f)"

    set --local literal_transitions
    set literal_transitions[1] "set inputs 10 3 8 9 5 11 12 6 14 4 15 16 7; set tos 7 3 7 7 7 3 4 3 7 4 3 7 3"
    set literal_transitions[2] "set inputs 11 12 4 3 15 6 7; set tos 3 4 4 3 3 3 3"
    set literal_transitions[5] "set inputs 2; set tos 6"
    set literal_transitions[6] "set inputs 1 13; set tos 7 7"

    set --local match_anything_transitions_from 4 3 1 2
    set --local match_anything_transitions_to 3 5 2 2

    set --local state 1
    set --local word_index 2
    while test $word_index -lt $COMP_CWORD
        set --local -- word $COMP_WORDS[$word_index]

        if set --query literal_transitions[$state] && test -n $literal_transitions[$state]
            set --local --erase inputs
            set --local --erase tos
            eval $literal_transitions[$state]

            if contains -- $word $literals
                set --local literal_matched 0
                for literal_id in (seq 1 (count $literals))
                    if test $literals[$literal_id] = $word
                        set --local index (contains --index -- $literal_id $inputs)
                        set state $tos[$index]
                        set word_index (math $word_index + 1)
                        set literal_matched 1
                        break
                    end
                end
                if test $literal_matched -ne 0
                    continue
                end
            end
        end

        if set --query match_anything_transitions_from[$state] && test -n $match_anything_transitions_from[$state]
            set --local index (contains --index -- $state $match_anything_transitions_from)
            set state $match_anything_transitions_to[$index]
            set word_index (math $word_index + 1)
            continue
        end

        return 1
    end

    if set --query literal_transitions[$state] && test -n $literal_transitions[$state]
        set --local --erase inputs
        set --local --erase tos
        eval $literal_transitions[$state]
        for literal_id in $inputs
            if test -n $descriptions[$literal_id]
                printf '%s\t%s\n' $literals[$literal_id] $descriptions[$literal_id]
            else
                printf '%s\n' $literals[$literal_id]
            end
        end
    end

    set command_states 4
    set command_ids 1
    if contains $state $command_states
        set --local index (contains --index $state $command_states)
        set --local function_id $command_ids[$index]
        set --local function_name _hyprpm_$function_id
        set --local --erase inputs
        set --local --erase tos
        $function_name "$COMP_WORDS[$COMP_CWORD]"
    end

    return 0
end

complete --command hyprpm --no-files --arguments "(_hyprpm)"
