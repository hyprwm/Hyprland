globber = run_command('sh', '-c', 'find . -name "*.cpp" | sort', check: true)
src = globber.stdout().strip().split('\n')

executable(
  'Hyprland',
  src,
  link_args: '-rdynamic',
  cpp_pch: 'pch/pch.hpp',
  dependencies: [
    server_protos,
    aquamarine,
    hyprcursor,
    hyprgraphics,
    hyprlang,
    hyprutils,
    dependency('gbm'),
    dependency('xcursor'),
    dependency('wayland-server'),
    dependency('wayland-client'),
    dependency('cairo'),
    dependency('libdrm'),
    dependency('egl'),
    dependency('xkbcommon'),
    dependency('libinput'),
    dependency('re2'),
    xcb_dep,
    xcb_composite_dep,
    xcb_errors_dep,
    xcb_icccm_dep,
    xcb_render_dep,
    xcb_res_dep,
    xcb_xfixes_dep,
    backtrace_dep,
    epoll_dep,
    inotify_dep,
    gio_dep,
    tracy,

    # Try to find canihavesomecoffee's udis86 using pkgconfig
    # vmt/udis86 does not provide a .pc file and won't be detected this way
    # Falls back to using the subproject through udis86.wrap
    dependency('udis86'),

    dependency('pixman-1'),
    dependency('gl', 'opengl'),
    dependency('threads'),
    dependency('pango'),
    dependency('pangocairo'),
    dependency('uuid'),
  ],
  install: true,
)

install_symlink(
  'hyprland',
  install_dir: get_option('bindir'),
  pointing_to: 'Hyprland',
)
