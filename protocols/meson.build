wayland_protos = dependency(
	'wayland-protocols',
	version: '>=1.43',
	fallback: 'wayland-protocols',
	default_options: ['tests=false'],
)

hyprland_protos = dependency(
	'hyprland-protocols',
	version: '>=0.6.4',
	fallback: 'hyprland-protocols',
)

wayland_protocol_dir = wayland_protos.get_variable('pkgdatadir')
hyprland_protocol_dir = hyprland_protos.get_variable('pkgdatadir')

hyprwayland_scanner_dep = dependency('hyprwayland-scanner', version: '>=0.3.10', native: true)
hyprwayland_scanner = find_program(
	hyprwayland_scanner_dep.get_variable('hyprwayland_scanner'),
	native: true,
)

protocols = [
	'wlr-gamma-control-unstable-v1.xml',
	'wlr-foreign-toplevel-management-unstable-v1.xml',
	'wlr-output-power-management-unstable-v1.xml',
	'input-method-unstable-v2.xml',
	'virtual-keyboard-unstable-v1.xml',
	'wlr-virtual-pointer-unstable-v1.xml',
	'wlr-output-management-unstable-v1.xml',
	'kde-server-decoration.xml',
	'wlr-layer-shell-unstable-v1.xml',
	'wayland-drm.xml',
	'wlr-data-control-unstable-v1.xml',
	'wlr-screencopy-unstable-v1.xml',
	'xx-color-management-v4.xml',
	'frog-color-management-v1.xml',
	hyprland_protocol_dir / 'protocols/hyprland-global-shortcuts-v1.xml',
	hyprland_protocol_dir / 'protocols/hyprland-toplevel-export-v1.xml',
	hyprland_protocol_dir / 'protocols/hyprland-toplevel-mapping-v1.xml',
	hyprland_protocol_dir / 'protocols/hyprland-focus-grab-v1.xml',
	hyprland_protocol_dir / 'protocols/hyprland-ctm-control-v1.xml',
	hyprland_protocol_dir / 'protocols/hyprland-surface-v1.xml',
	hyprland_protocol_dir / 'protocols/hyprland-lock-notify-v1.xml',
	wayland_protocol_dir / 'staging/tearing-control/tearing-control-v1.xml',
	wayland_protocol_dir / 'staging/fractional-scale/fractional-scale-v1.xml',
	wayland_protocol_dir / 'unstable/xdg-output/xdg-output-unstable-v1.xml',
	wayland_protocol_dir / 'staging/cursor-shape/cursor-shape-v1.xml',
	wayland_protocol_dir / 'unstable/idle-inhibit/idle-inhibit-unstable-v1.xml',
	wayland_protocol_dir / 'unstable/relative-pointer/relative-pointer-unstable-v1.xml',
	wayland_protocol_dir / 'unstable/xdg-decoration/xdg-decoration-unstable-v1.xml',
	wayland_protocol_dir / 'staging/alpha-modifier/alpha-modifier-v1.xml',
	wayland_protocol_dir / 'staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml',
	wayland_protocol_dir / 'unstable/pointer-gestures/pointer-gestures-unstable-v1.xml',
	wayland_protocol_dir / 'unstable/keyboard-shortcuts-inhibit/keyboard-shortcuts-inhibit-unstable-v1.xml',
	wayland_protocol_dir / 'unstable/text-input/text-input-unstable-v3.xml',
	wayland_protocol_dir / 'unstable/text-input/text-input-unstable-v1.xml',
	wayland_protocol_dir / 'unstable/pointer-constraints/pointer-constraints-unstable-v1.xml',
	wayland_protocol_dir / 'staging/xdg-activation/xdg-activation-v1.xml',
	wayland_protocol_dir / 'staging/ext-idle-notify/ext-idle-notify-v1.xml',
	wayland_protocol_dir / 'staging/ext-session-lock/ext-session-lock-v1.xml',
	wayland_protocol_dir / 'stable/tablet/tablet-v2.xml',
	wayland_protocol_dir / 'stable/presentation-time/presentation-time.xml',
	wayland_protocol_dir / 'stable/xdg-shell/xdg-shell.xml',
	wayland_protocol_dir / 'unstable/primary-selection/primary-selection-unstable-v1.xml',
	wayland_protocol_dir / 'staging/xwayland-shell/xwayland-shell-v1.xml',
	wayland_protocol_dir / 'stable/viewporter/viewporter.xml',
	wayland_protocol_dir / 'stable/linux-dmabuf/linux-dmabuf-v1.xml',
	wayland_protocol_dir / 'staging/drm-lease/drm-lease-v1.xml',
	wayland_protocol_dir / 'staging/linux-drm-syncobj/linux-drm-syncobj-v1.xml',
	wayland_protocol_dir / 'staging/xdg-dialog/xdg-dialog-v1.xml',
	wayland_protocol_dir / 'staging/single-pixel-buffer/single-pixel-buffer-v1.xml',
	wayland_protocol_dir / 'staging/security-context/security-context-v1.xml',
	wayland_protocol_dir / 'staging/content-type/content-type-v1.xml',
	wayland_protocol_dir / 'staging/color-management/color-management-v1.xml',
	wayland_protocol_dir / 'staging/xdg-toplevel-tag/xdg-toplevel-tag-v1.xml',
]

wl_protocols = []
foreach protocol : protocols
	wl_protocols += custom_target(
		protocol.underscorify(),
		input: protocol,
		install: true,
		install_dir: [false, join_paths(get_option('includedir'), 'hyprland/protocols')],
		output: ['@BASENAME@.cpp', '@BASENAME@.hpp'],
		command: [hyprwayland_scanner, '@INPUT@', '@OUTDIR@'],
	)
endforeach

# wayland.xml generation
wayland_scanner = dependency('wayland-scanner', native: true)
wayland_scanner_datadir = wayland_scanner.get_variable('pkgdatadir')

wayland_xml = wayland_scanner_datadir / 'wayland.xml'
wayland_protocol = custom_target(
	wayland_xml.underscorify(),
	input: wayland_xml,
	install: true,
	install_dir: [false, join_paths(get_option('includedir'), 'hyprland/protocols')],
	output: ['@BASENAME@.cpp', '@BASENAME@.hpp'],
	command: [hyprwayland_scanner, '--wayland-enums', '@INPUT@', '@OUTDIR@'],
)

lib_server_protos = static_library(
	'server_protos',
	wl_protocols + wayland_protocol,
)

server_protos = declare_dependency(
	link_with: lib_server_protos,
	sources: wl_protocols + wayland_protocol,
)
