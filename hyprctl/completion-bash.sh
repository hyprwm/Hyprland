# hyprctl.completion
_hyprctl() {
  # Define valid options and arguments based on your program's functionality
  local options="-h -j -r --batch --instance"
  local commands="activewindow activeworkspace binds clients configerrors cursorpos decorations devices dismissnotify dispatch getoption globalshortcuts hyprpaper instances keyword kill layers layouts monitors notify output plugin reload rollinglog setcursor seterror setprop splash switchxkblayout systeminfo version workspacerules workspaces"

  # Complete based on current word position
  local cur="${COMP_WORDS[COMP_CWORD]}"
  local prev="${COMP_WORDS[COMP_CWORD-1]}"

  if [[ $COMP_CWORD -eq 1 ]]; then
    # Complete the first word: program name or subcommand using fuzzy matching
    local matches=()
    for cmd in $options $commands; do
      if [[ $cmd =~ ^${cur} ]]; then
        matches+=($cmd)
      fi
    done
    COMPREPLY=("${matches[@]}")
  elif [[ $COMP_CWORD -eq 2 ]]; then
    # Complete subcommand arguments based on the subcommand
    case $prev in
      # ... Add specific completions for other subcommands as needed ...
      *)
        COMPREPLY=()  # No further completions for unknown subcommands
        ;;
    esac
  fi
}

complete -F _hyprctl hyprctl