globber = run_command('sh', '-c', 'find . -name "*.cpp" | sort', check: true)
src = globber.stdout().strip().split('\n')

executable(
  'hyprpm',
  src,
  dependencies: [
    dependency('hyprutils', version: '>= 0.1.1'),
    dependency('threads'),
    dependency('tomlplusplus'),
    dependency('glaze', method: 'cmake'),
  ],
  install: true,
)

install_data(
  '../hyprpm.bash',
  install_dir: join_paths(get_option('datadir'), 'bash-completion/completions'),
  install_tag: 'runtime',
  rename: 'hyprpm',
)
install_data(
  '../hyprpm.fish',
  install_dir: join_paths(get_option('datadir'), 'fish/vendor_completions.d'),
  install_tag: 'runtime',
)
install_data(
  '../hyprpm.zsh',
  install_dir: join_paths(get_option('datadir'), 'zsh/site-functions'),
  install_tag: 'runtime',
  rename: '_hyprpm',
)
