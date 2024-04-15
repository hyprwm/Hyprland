executable('hyprctl', 'main.cpp',
  install: true
)

install_data('hyprctl.bash', install_dir: join_paths(get_option('datadir'), 'bash-completion'), install_tag: 'runtime', rename: 'hyprctl')
install_data('hyprctl.fish', install_dir: join_paths(get_option('datadir'), 'fish/vendor_completions.d'), install_tag: 'runtime')
install_data('hyprctl.zsh', install_dir: join_paths(get_option('datadir'), 'zsh/site-functions'), install_tag: 'runtime', rename: '_hyprctl')
