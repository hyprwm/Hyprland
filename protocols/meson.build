wayland_protos = dependency('wayland-protocols',
	version: '>=1.32',
	fallback: 'wayland-protocols',
	default_options: ['tests=false'],
)

hyprland_protos = dependency('hyprland-protocols',
	version: '>=0.2',
  fallback: 'hyprland-protocols',
)

wl_protocol_dir = wayland_protos.get_variable('pkgdatadir')
hl_protocol_dir = hyprland_protos.get_variable('pkgdatadir')

wayland_scanner_dep = dependency('wayland-scanner', native: true)
wayland_scanner = find_program(
	wayland_scanner_dep.get_variable('wayland_scanner'),
	native: true,
)
hyprwayland_scanner_dep = dependency('hyprwayland-scanner', version: '>=0.3.8', native: true)
hyprwayland_scanner = find_program(
	hyprwayland_scanner_dep.get_variable('hyprwayland_scanner'),
	native: true,
)

protocols = [
  [wl_protocol_dir, 'unstable/text-input/text-input-unstable-v1.xml'],
  ['wlr-screencopy-unstable-v1.xml'],
  [hl_protocol_dir, 'protocols/hyprland-toplevel-export-v1.xml'],
  [hl_protocol_dir, 'protocols/hyprland-global-shortcuts-v1.xml']
]

new_protocols = [
  ['wlr-gamma-control-unstable-v1.xml'],
  ['wlr-foreign-toplevel-management-unstable-v1.xml'],
  ['wlr-output-power-management-unstable-v1.xml'],
  ['input-method-unstable-v2.xml'],
  ['virtual-keyboard-unstable-v1.xml'],
  ['wlr-virtual-pointer-unstable-v1.xml'],
  ['wlr-output-management-unstable-v1.xml'],
  ['kde-server-decoration.xml'],
  ['wlr-layer-shell-unstable-v1.xml'],
  ['wayland-drm.xml'],
  ['wlr-data-control-unstable-v1.xml'],
  [hl_protocol_dir, 'protocols/hyprland-focus-grab-v1.xml'],
  [wl_protocol_dir, 'staging/tearing-control/tearing-control-v1.xml'],
  [wl_protocol_dir, 'staging/fractional-scale/fractional-scale-v1.xml'],
  [wl_protocol_dir, 'unstable/xdg-output/xdg-output-unstable-v1.xml'],
  [wl_protocol_dir, 'staging/cursor-shape/cursor-shape-v1.xml'],
  [wl_protocol_dir, 'unstable/idle-inhibit/idle-inhibit-unstable-v1.xml'],
  [wl_protocol_dir, 'unstable/relative-pointer/relative-pointer-unstable-v1.xml'],
  [wl_protocol_dir, 'unstable/xdg-decoration/xdg-decoration-unstable-v1.xml'],
  [wl_protocol_dir, 'staging/alpha-modifier/alpha-modifier-v1.xml'],
  [wl_protocol_dir, 'staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml'],
  [wl_protocol_dir, 'unstable/pointer-gestures/pointer-gestures-unstable-v1.xml'],
  [wl_protocol_dir, 'unstable/keyboard-shortcuts-inhibit/keyboard-shortcuts-inhibit-unstable-v1.xml'],
  [wl_protocol_dir, 'unstable/text-input/text-input-unstable-v3.xml'],
  [wl_protocol_dir, 'unstable/pointer-constraints/pointer-constraints-unstable-v1.xml'],
  [wl_protocol_dir, 'staging/xdg-activation/xdg-activation-v1.xml'],
  [wl_protocol_dir, 'staging/ext-idle-notify/ext-idle-notify-v1.xml'],
  [wl_protocol_dir, 'staging/ext-session-lock/ext-session-lock-v1.xml'],
  [wl_protocol_dir, 'stable/tablet/tablet-v2.xml'],
  [wl_protocol_dir, 'stable/presentation-time/presentation-time.xml'],
  [wl_protocol_dir, 'stable/xdg-shell/xdg-shell.xml'],
  [wl_protocol_dir, 'unstable/primary-selection/primary-selection-unstable-v1.xml'],
  [wl_protocol_dir, 'staging/xwayland-shell/xwayland-shell-v1.xml'],
  [wl_protocol_dir, 'stable/viewporter/viewporter.xml'],
  [wl_protocol_dir, 'stable/linux-dmabuf/linux-dmabuf-v1.xml'],
  [wl_protocol_dir, 'staging/drm-lease/drm-lease-v1.xml'],
  [wl_protocol_dir, 'staging/linux-drm-syncobj/linux-drm-syncobj-v1.xml'],
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
		install: true,
		install_dir: join_paths(get_option('includedir'), 'hyprland/protocols'),
		output: '@BASENAME@-protocol.h',
		command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@'],
	)
endforeach

new_wl_protos = []
foreach p : new_protocols
	xml = join_paths(p)
	new_wl_protos += custom_target(
		xml.underscorify(),
		input: xml,
		install: true,
		install_dir: [false, join_paths(get_option('includedir'), 'hyprland/protocols')],
		output: ['@BASENAME@.cpp', '@BASENAME@.hpp'],
		command: [hyprwayland_scanner, '@INPUT@', '@OUTDIR@'],
	)
endforeach

wayland_server_dep = dependency('wayland-server', version: '>=1.20.0')
wayland_server_dir = wayland_server_dep.get_variable('pkgdatadir')

wl_server_protos = [
	wayland_server_dir / 'wayland.xml'
]
wl_server_protos_gen = []
foreach p : wl_server_protos
	wl_server_protos_gen += custom_target(
		p.underscorify(),
		input: p,
		install: true,
		install_dir: [false, join_paths(get_option('includedir'), 'hyprland/protocols')],
		output: ['@BASENAME@.cpp', '@BASENAME@.hpp'],
		command: [hyprwayland_scanner, '--wayland-enums', '@INPUT@', '@OUTDIR@'],
	)
endforeach

lib_server_protos = static_library(
	'server_protos',
	wl_protos_src + wl_protos_headers + new_wl_protos + wl_server_protos_gen,
	dependencies: wayland_server_dep.partial_dependency(compile_args: true),
)

server_protos = declare_dependency(
	link_with: lib_server_protos,
	sources: wl_protos_headers + new_wl_protos,
)
