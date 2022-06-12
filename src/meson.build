src = [
  'config/ConfigManager.cpp',
  'debug/HyprCtl.cpp',
  'debug/HyprDebugOverlay.cpp',
  'debug/Log.cpp',
  'events/Devices.cpp',
  'events/Layers.cpp',
  'events/Misc.cpp',
  'events/Monitors.cpp',
  'events/Popups.cpp',
  'events/Windows.cpp',
  'helpers/Color.cpp',
  'helpers/Vector2D.cpp',
  'helpers/WLListener.cpp',
  'helpers/AnimatedVariable.cpp',
  'helpers/BezierCurve.cpp',
  'helpers/MiscFunctions.cpp',
  'helpers/SubsurfaceTree.cpp',
  'helpers/WLClasses.cpp',
  'helpers/Workspace.cpp',
  'hyprerror/HyprError.cpp',
  'layout/DwindleLayout.cpp',
  'managers/LayoutManager.cpp',
  'managers/ThreadManager.cpp',
  'managers/AnimationManager.cpp',
  'managers/EventManager.cpp',
  'managers/KeybindManager.cpp',
  'managers/XWaylandManager.cpp',
  'managers/input/InputManager.cpp',
  'managers/input/Tablets.cpp',
  'render/Shader.cpp',
  'render/Texture.cpp',
  'render/Framebuffer.cpp',
  'render/OpenGL.cpp',
  'render/Renderer.cpp',
  'render/decorations/CHyprGroupBarDecoration.cpp',
  'render/decorations/IHyprWindowDecoration.cpp',
  'wlrunstable/wlr_ext_workspace_v1.cpp',
  'Compositor.cpp',
  'Window.cpp',
  'init/initHelpers.cpp',
  'main.cpp'
]

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
