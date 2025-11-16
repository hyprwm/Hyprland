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
    'b_lto=false',
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
    '-Wno-pointer-arith',
    datarootdir,
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
hyprgraphics = dependency('hyprgraphics', version: '>=0.1.6')
hyprlang = dependency('hyprlang', version: '>=0.3.2')
hyprutils = dependency('hyprutils', version: '>=0.8.2')

aq_ver_list = aquamarine.version().split('.')
git = find_program('git', required: false)

if git.found()
  git_hash = run_command(git, 'rev-parse', 'HEAD').stdout().strip()
  git_branch = run_command(git, 'branch', '--show-current').stdout().strip()
  git_message = run_command(git, 'show', '-s', '--format=%s', '--no-show-signature').stdout().strip()
  git_date = run_command(git, 'show', '-s', '--format=%cd', '--date=local', '--no-show-signature').stdout().strip()
  git_dirty = run_command(git, 'diff-index', '--quiet', 'HEAD', '--', check: false).returncode() != 0 ? 'dirty' : 'clean'
  git_tag = run_command(git, 'describe', '--tags').stdout().strip()
  git_commits = run_command(git, 'rev-list', '--count', 'HEAD').stdout().strip()
else
  git_hash = 'unknown'
  git_branch = 'unknown'
  git_message = 'unknown'
  git_date = 'unknown'
  git_dirty = 'unknown'
  git_tag = 'unknown'
  git_commits = '0'
endif

cfg = configuration_data()
cfg.set('GIT_COMMIT_HASH', git_hash)
cfg.set('GIT_BRANCH', git_branch)
cfg.set('GIT_COMMIT_MESSAGE', git_message)
cfg.set('GIT_COMMIT_DATE', git_date)
cfg.set('GIT_DIRTY', git_dirty)
cfg.set('GIT_TAG', git_tag)
cfg.set('GIT_COMMITS', git_commits)
cfg.set('AQUAMARINE_VERSION', aquamarine.version())
cfg.set('AQUAMARINE_VERSION_MAJOR', aq_ver_list[0])
cfg.set('AQUAMARINE_VERSION_MINOR', aq_ver_list[1])
cfg.set('AQUAMARINE_VERSION_PATCH', aq_ver_list[2])
cfg.set('HYPRLANG_VERSION', hyprlang.version())
cfg.set('HYPRUTILS_VERSION', hyprutils.version())
cfg.set('HYPRCURSOR_VERSION', hyprcursor.version())
cfg.set('HYPRGRAPHICS_VERSION', hyprgraphics.version())

version_h = configure_file(
  input: 'src/version.h.in',
  output: 'version.h',
  configuration: cfg
)

install_headers(version_h, subdir: 'hyprland/src')

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
epoll_dep = dependency('epoll-shim', required: false)
inotify_dep = dependency('libinotify', required: false)
re2 = dependency('re2', required: true)

systemd_option = get_option('systemd')
systemd = dependency('systemd', required: systemd_option)
systemd_option.enable_auto_if(systemd.found())
if (systemd_option.enabled())
  add_project_arguments('-DUSES_SYSTEMD', language: 'cpp')
  subdir('systemd')
endif

if get_option('buildtype') == 'debug'
  add_project_arguments('-DHYPRLAND_DEBUG', language: 'cpp')
endif

run_command('sh', '-c', 'scripts/generateShaderIncludes.sh', check: true)

globber = run_command('find', 'src', '-name', '*.h*', '-o', '-name', '*.inc', check: true)
headers = globber.stdout().strip().split('\n')
foreach file : headers
  install_headers(file, subdir: 'hyprland', preserve_path: true)
endforeach
install_headers(version_h, subdir: 'src')

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

pkg_install_dir = join_paths(get_option('datadir'), 'pkgconfig')
import('pkgconfig').generate(
  name: 'Hyprland',
  filebase: 'hyprland',
  url: 'https://github.com/hyprwm/Hyprland',
  description: 'Hyprland header files',
  install_dir: pkg_install_dir,
  subdirs: ['', 'hyprland/protocols', 'hyprland'],
)
