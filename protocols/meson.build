wayland_protos = dependency('wayland-protocols',
	version: '>=1.25',
	fallback: 'wayland-protocols',
	default_options: ['tests=false'],
)
wl_protocol_dir = wayland_protos.get_variable('pkgdatadir')

wayland_scanner_dep = dependency('wayland-scanner', native: true)
wayland_scanner = find_program(
	wayland_scanner_dep.get_variable('wayland_scanner'),
	native: true,
)

protocols = [
  [wl_protocol_dir, 'stable/xdg-shell/xdg-shell.xml'],
  ['wlr-layer-shell-unstable-v1.xml'],
  ['ext-workspace-unstable-v1.xml'],
  ['pointer-constraints-unstable-v1.xml'],
  ['tablet-unstable-v2.xml'],
  ['idle.xml']
]
wl_protos_src = []
wl_protos_headers = []
foreach p : protocols
	xml = join_paths(p)
	wl_protos_src += custom_target(
		xml.underscorify() + '_server_c',
		input: xml,
		output: '@BASENAME@-protocol.c',
		command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
	)
	wl_protos_headers += custom_target(
		xml.underscorify() + '_server_h',
		input: xml,
		output: '@BASENAME@-protocol.h',
		command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@'],
	)
endforeach

wayland_server = dependency('wayland-server', version: '>=1.20.0')

lib_server_protos = static_library(
	'server_protos',
	wl_protos_src + wl_protos_headers,
	dependencies: wayland_server.partial_dependency(compile_args: true),
)

server_protos = declare_dependency(
	link_with: lib_server_protos,
	sources: wl_protos_headers,
)
