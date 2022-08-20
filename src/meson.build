globber = run_command('find', '-name', '*.cpp', check: true)
src = globber.stdout().strip().split('\n')

executable('Hyprland', src,
  cpp_args: ['-DWLR_USE_UNSTABLE'],
  dependencies: [
    server_protos,
    wlroots,
    dependency('wayland-server'),
    dependency('wayland-client'),
    dependency('cairo'),
    dependency('pango'),
    dependency('pangocairo'),
    dependency('libdrm'),
    dependency('egl'),
    dependency('xkbcommon'),
    dependency('libinput'),
    xcb_dep,

    dependency('pixman-1'),
    dependency('GL'),
    dependency('threads')
  ],
  install : true
)
