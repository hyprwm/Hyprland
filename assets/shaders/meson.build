globber = run_command('sh', '-c', 'find . -type f -not -name "*.build"', check: true)
files = globber.stdout().strip().split('\n')

foreach file : files
  install_data(
    file,
    install_dir: join_paths(get_option('datadir'), 'hypr', 'shaders'),
    install_tag: 'runtime',
  )
endforeach
