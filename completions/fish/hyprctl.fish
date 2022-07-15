set -l commands monitors workspaces clients activewindow layers devices dispatch keyword version kill reload

complete -c hyprctl -f

complete -c hyprctl -n "not __fish_seen_subcommand_from $commands" -a monitors -d 'lists all the outputs with their properties'
complete -c hyprctl -n "not __fish_seen_subcommand_from $commands" -a workspaces -d 'lists all workspaces with their properties'
complete -c hyprctl -n "not __fish_seen_subcommand_from $commands" -a clients -d 'lists all windows with their properties'
complete -c hyprctl -n "not __fish_seen_subcommand_from $commands" -a activewindow -d 'gets the active window name'
complete -c hyprctl -n "not __fish_seen_subcommand_from $commands" -a layers -d 'WARNING: Crashes Hyprland often! lists all the layers'
complete -c hyprctl -n "not __fish_seen_subcommand_from $commands" -a devices -d 'lists all connected keyboards and mice'
complete -c hyprctl -n "not __fish_seen_subcommand_from $commands" -a dispatch -d 'call a keybind dispatcher'
complete -c hyprctl -n "not __fish_seen_subcommand_from $commands" -a keyword -d 'call a config keyword dynamically'
complete -c hyprctl -n "not __fish_seen_subcommand_from $commands" -a version -d 'prints the hyprland version'
complete -c hyprctl -n "not __fish_seen_subcommand_from $commands" -a kill -d 'kill an app by clicking on it'
complete -c hyprctl -n "not __fish_seen_subcommand_from $commands" -a reload -d 'issue a reload to force reload the config'

complete -c hyprctl -l batch -d 'specify a batch of commands to execute ";" separates the commands'

complete -c hyprctl -n "__fish_seen_subcommand_from dispatch" -a "exec killactive workspace movetoworkspace movetoworkspacesilent togglefloating fullscreen pseudo movefocus movewindow resizeactive moveactive cyclenext focuswindowbyclass focusmonitor splitratio movecursortocorner workspaceopt exit forcerendererreload movecurrentworkspacetomonitor moveworkspacetomonitor togglespecialworkspace"
