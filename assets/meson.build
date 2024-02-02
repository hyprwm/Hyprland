wallpapers = ['0', '1', '2']

foreach type : wallpapers
  install_data(f'wall@type@.png', install_dir: join_paths(get_option('datadir'), 'hyprland'), install_tag: 'runtime')
endforeach

install_data('hyprland-portals.conf', install_dir: join_paths(get_option('datadir'), 'xdg-desktop-portal'), install_tag: 'runtime')
