globber = run_command('sh', '-c', 'find . -name "*.cpp" | sort', check: true)
src = globber.stdout().strip().split('\n')

dependencies = [
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
    dependency('pixman-1'),
    dependency('gl', 'opengl'),
    dependency('threads'),
    dependency('pango'),
    dependency('pangocairo')
]

if get_option('plugins').enabled()
  dependencies += [udis86]
else
tmp = []
foreach item : src
  if not item.contains('plugins')
    tmp += item
  endif
endforeach
src = tmp
endif

executable('Hyprland', src,
  cpp_args: ['-DWLR_USE_UNSTABLE'],
  link_args: '-rdynamic',
  cpp_pch: 'pch/pch.hpp',
  dependencies: dependencies,
  install : true
)
