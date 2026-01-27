function _hyprpm_1
    set 1 $argv[1]
    hyprpm list | awk '/Plugin/{print $4}'
end

function _hyprpm_2
    set 1 $argv[1]
    hyprpm list | awk '/Repository/{print $4}' | sed 's/:$//'
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

    set literals "--no-shallow" "-n" "::=" "disable" "list" "--help" "update" "add" "--verbose" "-v" "--force" "-s" "remove" "enable" "--notify" "-h" "reload" "-f"

    set descriptions
    set descriptions[1] "Disable shallow cloning of Hyprland sources"
    set descriptions[2] "Send a hyprland notification for important events (e.g. load fail)"
    set descriptions[4] "Unload a plugin"
    set descriptions[5] "List all installed plugins"
    set descriptions[6] "Show help menu"
    set descriptions[7] "Check and update all plugins if needed"
    set descriptions[8] "Install a new plugin repository from git"
    set descriptions[9] "Enable too much logging"
    set descriptions[10] "Enable too much logging"
    set descriptions[11] "Force an operation ignoring checks (e.g. update -f)"
    set descriptions[12] "Disable shallow cloning of Hyprland sources"
    set descriptions[13] "Remove a plugin repository"
    set descriptions[14] "Load a plugin"
    set descriptions[15] "Send a hyprland notification for important events (e.g. load fail)"
    set descriptions[16] "Show help menu"
    set descriptions[17] "Reload all plugins"
    set descriptions[18] "Force an operation ignoring checks (e.g. update -f)"

    set literal_transitions
    set literal_transitions[1] "set inputs 1 4 5 9 10 7 8 12 6 11 13 14 16 17 18; set tos 8 4 5 8 8 5 5 8 8 8 3 4 8 5 8"
    set literal_transitions[2] "set inputs 13 14 4 5 17 7 8; set tos 3 4 4 5 5 5 5"
    set literal_transitions[6] "set inputs 3; set tos 7"
    set literal_transitions[7] "set inputs 2 15; set tos 8 8"

    set match_anything_transitions_from 2 5 4 3 1
    set match_anything_transitions_to 2 6 5 5 2

    set state 1
    set word_index 2
    while test $word_index -lt $COMP_CWORD
        set -- word $COMP_WORDS[$word_index]

        if set --query literal_transitions[$state] && test -n $literal_transitions[$state]
            set --erase inputs
            set --erase tos
            eval $literal_transitions[$state]

            if contains -- $word $literals
                set literal_matched 0
                for literal_id in (seq 1 (count $literals))
                    if test $literals[$literal_id] = $word
                        set index (contains --index -- $literal_id $inputs)
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
            set index (contains --index -- $state $match_anything_transitions_from)
            set state $match_anything_transitions_to[$index]
            set word_index (math $word_index + 1)
            continue
        end

        return 1
    end

    if set --query literal_transitions[$state] && test -n $literal_transitions[$state]
        set --erase inputs
        set --erase tos
        eval $literal_transitions[$state]
        for literal_id in $inputs
            if test -n $descriptions[$literal_id]
                printf '%s\t%s\n' $literals[$literal_id] $descriptions[$literal_id]
            else
                printf '%s\n' $literals[$literal_id]
            end
        end
    end

    set command_states 4 3
    set command_ids 1 2
    if contains $state $command_states
        set index (contains --index $state $command_states)
        set function_id $command_ids[$index]
        set function_name _hyprpm_$function_id
        set --erase inputs
        set --erase tos
        $function_name "$COMP_WORDS[$COMP_CWORD]"
    end

    return 0
end

complete --command hyprpm --no-files --arguments "(_hyprpm)"
