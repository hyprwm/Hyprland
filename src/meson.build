globber = run_command('find', '.', '-name', '*.cpp', check: true)
src = globber.stdout().strip().split('\n')

executable('Hyprland', src,
  cpp_args: ['-DWLR_USE_UNSTABLE'],
  dependencies: [
    server_protos,
    dependency('wayland-server'),
    dependency('wayland-client'),
    wlroots.get_variable('wlroots'),
    dependency('cairo'),
    dependency('libdrm'),
    dependency('egl'),
    dependency('xkbcommon'),
    dependency('libinput'),
    xcb_dep,
    backtrace_dep,
    systemd_dep,
    udis86,

    dependency('pixman-1'),
    dependency('gl', 'opengl'),
    dependency('threads')
  ],
  install : true
)
