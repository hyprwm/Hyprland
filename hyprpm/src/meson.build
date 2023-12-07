globber = run_command('sh', '-c', 'find . -name "*.cpp" | sort', check: true)
src = globber.stdout().strip().split('\n')

executable('hyprpm', src,
  dependencies: [
    dependency('threads'),
    dependency('tomlplusplus')
  ],
  install : true
)
