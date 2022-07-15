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

pointer-constraints-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		protocols/pointer-constraints-unstable-v1.xml $@

pointer-constraints-unstable-v1-protocol.c:
	$(WAYLAND_SCANNER) private-code \
		protocols/pointer-constraints-unstable-v1.xml $@

pointer-constraints-unstable-v1-protocol.o: pointer-constraints-unstable-v1-protocol.h

tablet-unstable-v2-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		protocols/tablet-unstable-v2.xml $@

tablet-unstable-v2-protocol.c:
	$(WAYLAND_SCANNER) private-code \
		protocols/tablet-unstable-v2.xml $@

tablet-unstable-v2-protocol.o: tablet-unstable-v2-protocol.h

idle-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		protocols/idle.xml $@

idle-protocol.c:
	$(WAYLAND_SCANNER) private-code \
		protocols/idle.xml $@

idle-protocol.o: idle-protocol.h

legacyrenderer:
	mkdir -p build && cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -DLEGACY_RENDERER:STRING=true -H./ -B./build -G Ninja
	cmake --build ./build --config Release --target all -j 10

legacyrendererdebug:
	mkdir -p build && cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Debug -DLEGACY_RENDERER:STRING=true -H./ -B./build -G Ninja
	cmake --build ./build --config Release --target all -j 10

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
	rm -rf ./subprojects/wlroots/build

all:
	make config
	make release
	cd ./hyprctl && make all && cd ..

install:
	make clear
	make fixwlr
	cd ./subprojects/wlroots && meson build/ --buildtype=release && ninja -C build/ && cp ./build/libwlroots.so.11032 /usr/lib/ && cd ../..
	make protocols
	make release
	cd hyprctl && make all && cd ..

	mkdir -p ${PREFIX}/share/wayland-sessions
	cp ./example/hyprland.desktop ${PREFIX}/share/wayland-sessions/
	mkdir -p ${PREFIX}/bin
	cp ./build/Hyprland ${PREFIX}/bin
	cp ./hyprctl/hyprctl ${PREFIX}/bin
	mkdir -p ${PREFIX}/share/hyprland
	cp ./assets/wall_2K.png ${PREFIX}/share/hyprland
	cp ./assets/wall_4K.png ${PREFIX}/share/hyprland
	cp ./assets/wall_8K.png ${PREFIX}/share/hyprland


uninstall:
	rm -f ${PREFIX}/share/wayland-sessions/hyprland.desktop
	rm -f ${PREFIX}/bin/Hyprland
	rm -f ${PREFIX}/bin/hyprctl
	rm -f /usr/lib/libwlroots.so.11032
	rm -rf ${PREFIX}/share/hyprland

protocols: xdg-shell-protocol.o wlr-layer-shell-unstable-v1-protocol.o wlr-screencopy-unstable-v1-protocol.o idle-protocol.o ext-workspace-unstable-v1-protocol.o pointer-constraints-unstable-v1-protocol.o tablet-unstable-v2-protocol.o

fixwlr:
	sed -i -E 's/(soversion = 11)([^032]|$$)/soversion = 11032/g' subprojects/wlroots/meson.build

	rm -rf ./subprojects/wlroots/build

config:
	make protocols

	make fixwlr

	cd subprojects/wlroots && meson ./build --prefix=/usr --buildtype=release
	cd subprojects/wlroots && ninja -C build/

	cd subprojects/wlroots && ninja -C build/ install
