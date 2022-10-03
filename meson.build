project('Hyprland', 'cpp', 'c',
  version : '0.15.0beta',
  default_options : [
    'warning_level=2',
    'default_library=static',
    'optimization=3',
    # 'cpp_std=c++23' # not yet supported by meson, as of version 0.63.0
    ])

# clang v14.0.6 uses C++2b instead of C++23, so we've gotta account for that
# replace the following with a project default option once meson gets support for C++23
cpp_compiler = meson.get_compiler('cpp')
if cpp_compiler.has_argument('-std=c++23')
  add_global_arguments('-std=c++23', language: 'cpp')
elif cpp_compiler.has_argument('-std=c++2b')
  add_global_arguments('-std=c++2b', language: 'cpp')
else
  error('Could not configure current C++ compiler (' + cpp_compiler.get_id() + ' ' + cpp_compiler.version() + ') with required C++ standard (C++23)')
endif

GIT_BRANCH = run_command('git', 'rev-parse', '--abbrev-ref', 'HEAD', check: false).stdout().strip()
GIT_COMMIT_HASH = run_command('git', 'rev-parse', 'HEAD', check: false).stdout().strip()
GIT_COMMIT_MESSAGE = run_command('bash', '-c', 'git show | head -n 5 | tail -n 1', check: false).stdout().strip()
GIT_DIRTY = run_command('bash', '-c', 'git diff-index --quiet HEAD -- || echo "dirty"', check: false).stdout().strip()

add_project_arguments(
  [
    '-Wno-unused-parameter',
    '-Wno-unused-value',
    '-Wno-missing-field-initializers',
    '-Wno-narrowing',

    f'-DGIT_BRANCH="@GIT_BRANCH@"',
    f'-DGIT_COMMIT_HASH="@GIT_COMMIT_HASH@"',
    f'-DGIT_COMMIT_MESSAGE="@GIT_COMMIT_MESSAGE@"',
    f'-DGIT_DIRTY="@GIT_DIRTY@"',
  ],
  language: 'cpp')

wlroots = subproject('wlroots', default_options: ['examples=false'])
have_xwlr = wlroots.get_variable('features').get('xwayland')
xcb_dep = dependency('xcb', required: get_option('xwayland'))

if get_option('xwayland').enabled() and not have_xwlr
	error('Cannot enable Xwayland in Hyprland: wlroots has been built without Xwayland support')
endif
have_xwayland = xcb_dep.found() and have_xwlr

if not have_xwayland
add_project_arguments('-DNO_XWAYLAND', language: 'cpp')
endif

if get_option('buildtype') == 'debug'
  add_project_arguments('-DHYPRLAND_DEBUG', language: 'cpp')
endif

subdir('protocols')
subdir('src')
subdir('hyprctl')
subdir('assets')
subdir('example')
subdir('docs')
