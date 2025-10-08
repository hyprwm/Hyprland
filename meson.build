project(
  'Hyprland',
  'cpp',
  'c',
  version: run_command('cat', join_paths(meson.project_source_root(), 'VERSION'), check: true).stdout().strip(),
  default_options: [
    'warning_level=2',
    'default_library=static',
    'optimization=3',
    'buildtype=release',
    'debug=false',
    'cpp_std=c++26',
  ],
  meson_version: '>= 1.1.0',
)

datarootdir = '-DDATAROOTDIR="' + get_option('prefix') / get_option('datadir') + '"'
add_project_arguments(
  [
    '-Wno-unused-parameter',
    '-Wno-unused-value',
    '-Wno-missing-field-initializers',
    '-Wno-narrowing',
    '-Wno-pointer-arith', datarootdir,
    '-DHYPRLAND_VERSION="' + meson.project_version() + '"',
  ],
  language: 'cpp',
)

cpp_compiler = meson.get_compiler('cpp')
if cpp_compiler.check_header('execinfo.h')
  add_project_arguments('-DHAS_EXECINFO', language: 'cpp')
endif

aquamarine = dependency('aquamarine', version: '>=0.9.3')
hyprcursor = dependency('hyprcursor', version: '>=0.1.7')
hyprgraphics = dependency('hyprgraphics', version: '>= 0.1.6')
hyprlang = dependency('hyprlang', version: '>= 0.3.2')
hyprutils = dependency('hyprutils', version: '>= 0.8.2')
aquamarine_version_list = aquamarine.version().split('.')
add_project_arguments(['-DAQUAMARINE_VERSION="@0@"'.format(aquamarine.version())], language: 'cpp')
add_project_arguments(['-DAQUAMARINE_VERSION_MAJOR=@0@'.format(aquamarine_version_list.get(0))], language: 'cpp')
add_project_arguments(['-DAQUAMARINE_VERSION_MINOR=@0@'.format(aquamarine_version_list.get(1))], language: 'cpp')
add_project_arguments(['-DAQUAMARINE_VERSION_PATCH=@0@'.format(aquamarine_version_list.get(2))], language: 'cpp')
add_project_arguments(['-DHYPRCURSOR_VERSION="@0@"'.format(hyprcursor.version())], language: 'cpp')
add_project_arguments(['-DHYPRGRAPHICS_VERSION="@0@"'.format(hyprgraphics.version())], language: 'cpp')
add_project_arguments(['-DHYPRLANG_VERSION="@0@"'.format(hyprlang.version())], language: 'cpp')
add_project_arguments(['-DHYPRUTILS_VERSION="@0@"'.format(hyprutils.version())], language: 'cpp')

xcb_dep = dependency('xcb', required: get_option('xwayland'))
xcb_composite_dep = dependency('xcb-composite', required: get_option('xwayland'))
xcb_errors_dep = dependency('xcb-errors', required: get_option('xwayland'))
xcb_icccm_dep = dependency('xcb-icccm', required: get_option('xwayland'))
xcb_render_dep = dependency('xcb-render', required: get_option('xwayland'))
xcb_res_dep = dependency('xcb-res', required: get_option('xwayland'))
xcb_xfixes_dep = dependency('xcb-xfixes', required: get_option('xwayland'))

gio_dep = dependency('gio-2.0', required: true)

if not xcb_dep.found()
  add_project_arguments('-DNO_XWAYLAND', language: 'cpp')
endif

backtrace_dep = cpp_compiler.find_library('execinfo', required: false)
epoll_dep = dependency('epoll-shim', required: false) # timerfd on BSDs
inotify_dep = dependency('libinotify', required: false) # inotify on BSDs

re2 = dependency('re2', required: true)

# Handle options
systemd_option = get_option('systemd')
systemd = dependency('systemd', required: systemd_option)
systemd_option.enable_auto_if(systemd.found())

if (systemd_option.enabled())
  message('Enabling systemd integration')
  add_project_arguments('-DUSES_SYSTEMD', language: 'cpp')
  subdir('systemd')
endif

if get_option('buildtype') == 'debug'
  add_project_arguments('-DHYPRLAND_DEBUG', language: 'cpp')
endif

# Generate hyprland version and populate version.h
run_command('sh', '-c', 'scripts/generateVersion.sh', check: true)
# Make shader files includable
run_command('sh', '-c', 'scripts/generateShaderIncludes.sh', check: true)

# Install headers
globber = run_command('find', 'src', '-name', '*.h*', '-o', '-name', '*.inc', check: true)
headers = globber.stdout().strip().split('\n')
foreach file : headers
  install_headers(file, subdir: 'hyprland', preserve_path: true)
endforeach

tracy = dependency('tracy', static: true, required: get_option('tracy_enable'))

if get_option('tracy_enable') and get_option('buildtype') != 'debugoptimized'
  warning('Profiling builds should set -- buildtype = debugoptimized')
endif



subdir('protocols')
subdir('src')
subdir('hyprctl')
subdir('assets')
subdir('example')
subdir('docs')

if get_option('hyprpm').enabled()
  subdir('hyprpm/src')
endif

# Generate hyprland.pc
pkg_install_dir = join_paths(get_option('datadir'), 'pkgconfig')

import('pkgconfig').generate(
  name: 'Hyprland',
  filebase: 'hyprland',
  url: 'https://github.com/hyprwm/Hyprland',
  description: 'Hyprland header files',
  install_dir: pkg_install_dir,
  subdirs: ['', 'hyprland/protocols', 'hyprland'],
)
