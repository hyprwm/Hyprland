globber = run_command('sh', '-c', 'find . -name "*.cpp" | sort', check: true)
src = globber.stdout().strip().split('\n')

executable('Hyprland', src,
  cpp_args: ['-DWLR_USE_UNSTABLE'],
  link_args: '-rdynamic',
  cpp_pch: 'pch/pch.hpp',
  dependencies: [
    server_protos,
    dependency('wayland-server'),
    dependency('wayland-client'),
    wlroots.get_variable('wlroots'),
    dependency('cairo'),
    dependency('hyprlang', version: '>= 0.3.2'),
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
    dependency('threads'),
    dependency('pango'),
    dependency('pangocairo')
  ],
  install : true
)
