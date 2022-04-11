include config.mk

CFLAGS += -I. -DWLR_USE_UNSTABLE -std=c99

WAYLAND_PROTOCOLS=$(shell pkg-config --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER=$(shell pkg-config --variable=wayland_scanner wayland-scanner)

PKGS = wlroots wayland-server xcb xkbcommon libinput
CFLAGS += $(foreach p,$(PKGS),$(shell pkg-config --cflags $(p)))
LDLIBS += $(foreach p,$(PKGS),$(shell pkg-config --libs $(p)))

xdg-shell-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

xdg-shell-protocol.c:
	$(WAYLAND_SCANNER) private-code \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

xdg-shell-protocol.o: xdg-shell-protocol.h

wlr-layer-shell-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		protocols/wlr-layer-shell-unstable-v1.xml $@

wlr-layer-shell-unstable-v1-protocol.c:
	$(WAYLAND_SCANNER) private-code \
		protocols/wlr-layer-shell-unstable-v1.xml $@

wlr-layer-shell-unstable-v1-protocol.o: wlr-layer-shell-unstable-v1-protocol.h

wlr-screencopy-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		protocols/wlr-screencopy-unstable-v1.xml $@

wlr-screencopy-unstable-v1-protocol.c:
	$(WAYLAND_SCANNER) private-code \
		protocols/wlr-screencopy-unstable-v1.xml $@

wlr-screencopy-unstable-v1-protocol.o: wlr-screencopy-unstable-v1-protocol.h

ext-workspace-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		protocols/ext-workspace-unstable-v1.xml $@

ext-workspace-unstable-v1-protocol.c:
	$(WAYLAND_SCANNER) private-code \
		protocols/ext-workspace-unstable-v1.xml $@

ext-workspace-unstable-v1-protocol.o: ext-workspace-unstable-v1-protocol.h

idle-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		protocols/idle.xml $@

idle-protocol.c:
	$(WAYLAND_SCANNER) private-code \
		protocols/idle.xml $@

idle-protocol.o: idle-protocol.h

release:
	mkdir -p build && cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -H./ -B./build -G Ninja
	cmake --build ./build --config Release --target all -j 10

debug:
	mkdir -p build && cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Debug -H./ -B./build -G Ninja
	cmake --build ./build --config Debug --target all -j 10

clear:
	rm -rf build
	rm -f *.o *-protocol.h *-protocol.c
	rm -f ./hyprctl/hyprctl

all:
	make config
	make release
	cd ./hyprctl && make all && cd ..

install:
	make all
	mkdir -p /usr/share/wayland-sessions
	cp ./example/hyprland.desktop /usr/share/wayland-sessions/
	cp ./build/Hyprland /usr/bin
	cp ./hyprctl/hyprctl /usr/bin
	mkdir -p /usr/share/hyprland
	cp ./assets/wall_2K.png /usr/share/hyprland
	cp ./assets/wall_4K.png /usr/share/hyprland
	cp ./assets/wall_8K.png /usr/share/hyprland

config: xdg-shell-protocol.o wlr-layer-shell-unstable-v1-protocol.o wlr-screencopy-unstable-v1-protocol.o idle-protocol.o ext-workspace-unstable-v1-protocol.o
