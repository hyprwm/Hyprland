install_data(                                                          
  'hyprland-systemd.desktop',                                                  
  install_dir: join_paths(get_option('datadir'), 'wayland-sessions'),  
  install_tag: 'runtime',                                              
)                                                                      

conf_data = configuration_data()
conf_data.set('PREFIX', get_option('prefix'))
conf_data.set('BINDIR', get_option('bindir'))
user_unit_dir = systemd.get_variable(pkgconfig: 'systemduserunitdir',
    pkgconfig_define: ['prefix', get_option('prefix')])

configure_file(
  configuration: conf_data,
  input: 'hyprland-session.service.in',
  output: '@BASENAME@',
  install_dir: user_unit_dir )
