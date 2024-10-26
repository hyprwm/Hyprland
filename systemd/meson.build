install_data(                                                          
  'hyprland-systemd.desktop',                                                  
  install_dir: join_paths(get_option('datadir'), 'wayland-sessions'),  
  install_tag: 'runtime',                                              
)                                                                      

prefix = get_option('prefix')
bindir = get_option('bindir')
conf_data = configuration_data()
conf_data.set('PREFIX', get_option('prefix'))
conf_data.set('BINDIR', get_option('bindir'))

configure_file(
  configuration: conf_data,
  input: 'hyprland-session.service.in',
  output: '@BASENAME@',
  install_dir: join_paths(get_option('libdir'), 'systemd/user')  )
