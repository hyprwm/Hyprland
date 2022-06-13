globber = run_command('find', '-name', '*.cpp', check: true)
src = globber.stdout().strip().split('\n')

wlroots = subproject('wlroots', default_options: ['examples=false'])
have_xwlr = wlroots.get_variable('features').get('xwayland')
xcb_dep = dependency('xcb', required: get_option('xwayland'))

if get_option('xwayland').enabled() and not have_xwlr
	error('Cannot enable Xwayland in Hyperland: wlroots has been built without Xwayland support')
endif
have_xwayland = xcb_dep.found() and have_xwlr

if not have_xwayland
add_project_arguments('-DNO_XWAYLAND', language: 'cpp')
endif

executable('Hyprland', src,
  cpp_args: ['-DWLR_USE_UNSTABLE'],
  dependencies: [
    server_protos,
    wlroots.get_variable('wlroots'),
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
