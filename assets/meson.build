wallpaper_types = ['', 'anime_', 'anime2_']

foreach type : wallpaper_types
  foreach size : [2, 4, 8]
    install_data(f'wall_@type@@size@K.png', install_dir: join_paths(get_option('datadir'), 'hyprland'), install_tag: 'runtime')
  endforeach
endforeach

install_data('hyprland-portals.conf', install_dir: join_paths(get_option('datadir'), 'xdg-desktop-portal'), install_tag: 'runtime')
