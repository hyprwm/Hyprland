pandoc = find_program('pandoc', required : get_option('man'))
build_man_pages = pandoc.found()

if build_man_pages
  foreach page : [
    'hyprctl',
    'Hyprland',
  ] custom_target(
      page + '_manpage',
      input : page + '.1.rst',
      output : page + '.1',
      command : [
        pandoc,
        '@INPUT@',
        '--standalone',
        '--variable=header:"{page} User Manual"',
        '--variable=section:1',
        '--from', 'rst',
        '--to', 'man',
        '-o', '@OUTPUT@'
      ],
      build_by_default : true,
      install : true,
      install_dir : join_paths(
        get_option('prefix'),
        get_option('mandir'),
        'man1',
      ),
      install_tag : 'man',
    )
  endforeach
endif
