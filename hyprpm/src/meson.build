globber = run_command('sh', '-c', 'find . -name "*.cpp" | sort', check: true)
src = globber.stdout().strip().split('\n')

executable('hyprpm', src,
  dependencies: [
    dependency('threads'),
    dependency('tomlplusplus')
  ],
  install : true
)

install_data('../hyprpm.bash', install_dir: join_paths(get_option('datadir'), 'bash-completion'), install_tag: 'runtime', rename: 'hyprpm')
install_data('../hyprpm.fish', install_dir: join_paths(get_option('datadir'), 'fish/vendor_completions.d'), install_tag: 'runtime')
install_data('../hyprpm.zsh', install_dir: join_paths(get_option('datadir'), 'zsh/site-functions'), install_tag: 'runtime', rename: '_hyprpm')
