# hyprctl.completion
_hyprctl() {
  # Define valid options and arguments based on your program's functionality
  local options="-h -j -r --batch --instance"
  local commands="activewindow activeworkspace binds clients configerrors cursorpos decorations devices dismissnotify dispatch getoption globalshortcuts hyprpaper instances keyword kill layers layouts monitors notify output plugin reload rollinglog setcursor seterror setprop splash switchxkblayout systeminfo version workspacerules workspaces"
  local dispatchers="exec execr pass killactive closewindow workspace movetoworkspace movetoworkspacesilent togglefloating setfloating settiled fullscreen fakefullscreen dpms pin movefocus movewindow swapwindow centerwindow resizeactive moveactive resizewindowpixel movewindowpixel cyclenext swapnext focuswindow focusmonitor splitratio toggleopaque movecursortocorner movecursor renameworkspace exit forcerendererreload movecurrentworkspacetomonitor focusworkspaceoncurrentmonitor moveworkspacetomonitor swapactiveworkspaces bringactivetotop alterzorder togglespecialworkspace focusurgentorlast togglegroup changegroupactive focuscurrentorlast lockgroups lockactivegroup moveintogroup moveoutofgroup movewindoworgroup movegroupwindow denywindowfromgroup setignoregrouplock global subma"
  # Complete based on current word position
  local cur="${COMP_WORDS[COMP_CWORD]}"
  local prev="${COMP_WORDS[COMP_CWORD-1]}"
  local prev2="${COMP_WORDS[COMP_CWORD-2]}"

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
    case $prev in
      dispatch)
        local matches=()
        for cmd in $dispatchers; do
          if [[ $cmd =~ ^${cur} ]]; then
            matches+=($cmd)
          fi
        done
        COMPREPLY=("${matches[@]}")
        ;;
      output)
        local output_options="create remove"
        local matches=()
        for cmd in $output_options; do
          if [[ $cmd =~ ^${cur} ]]; then
            matches+=($cmd)
          fi
        done
        COMPREPLY=("${matches[@]}")
        ;;
        switchxkblayout)
        local kb_layouts=$(localectl list-x11-keymap-layouts | /bin/cat)
        local matches=()
        for cmd in $kb_layouts; do
          if [[ $cmd =~ ^${cur} ]]; then
            matches+=($cmd)
          fi
        done
        COMPREPLY=("${matches[@]}")
        ;;
      seterror)
        if [[ "disable" =~ ^${cur} ]]; then
            COMPREPLY=("disable")
        fi
        ;;
      setprop)
        local prop_options="animationstyle rounding bordersize forcenoblur forceopaque forceopaqueoverriden forceallowsinput forcenoanims forcenoborder forcenodim forcenoshadow nofocus windowdancecompat nomaxsize minsize maxsize dimaround keepaspectratio alphaoverride alpha alphainactiveoverride alphainactive activebordercolor inactivebordercolo"
        local matches=()
        for cmd in $prop_options; do
          if [[ $cmd =~ ^${cur} ]]; then
            matches+=($cmd)
          fi
        done
        COMPREPLY=("${matches[@]}")
        ;;
        notify)
        local notify_icons="0 1 2 3 4 5"
        local matches=()
        for cmd in $notify_icons; do
          if [[ $cmd =~ ^${cur} ]]; then
            matches+=($cmd)
          fi
        done
        COMPREPLY=("${matches[@]}")
        ;;
        *)
        COMPREPLY=()
        ;;
    esac

    elif [[ $COMP_CWORD -eq 3 ]]; then
    # Complete arguments for specific subcommands
    case $prev2 in
      output)
        case $prev in
          create)
            local backend_options="x11 wayland auto headless"
            local matches=()
            for cmd in $backend_options; do
              if [[ $cmd =~ ^${cur} ]]; then
                matches+=($cmd)
              fi
            done
            COMPREPLY=("${matches[@]}")
            ;;
          remove)
            local monitors=$(hyprctl monitors | grep "Monitor" | awk '{print $2}')
            local matches=()
            for cmd in $monitors; do
              if [[ $cmd =~ ^${cur} ]]; then
                matches+=($cmd)
              fi
            done
            COMPREPLY=("${matches[@]}")
            ;;
        esac
        ;;
      *)
        COMPREPLY=() 
        ;;
    esac
  fi
}

complete -F _hyprctl hyprctl



