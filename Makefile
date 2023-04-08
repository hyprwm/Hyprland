include config.mk

CFLAGS += -I. -DWLR_USE_UNSTABLE -std=c99

WAYLAND_PROTOCOLS=$(shell pkg-config --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER=$(shell pkg-config --variable=wayland_scanner wayland-scanner)

PKGS = wlroots wayland-server xcb xkbcommon libinput
CFLAGS += $(foreach p,$(PKGS),$(shell pkg-config --cflags $(p)))
LDLIBS += $(foreach p,$(PKGS),$(shell pkg-config --libs $(p)))

DATE=$(shell date "+%d %b %Y")

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

wlr-output-power-management-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		protocols/wlr-output-power-management-unstable-v1.xml $@

wlr-output-power-management-unstable-v1-protocol.c:
	$(WAYLAND_SCANNER) private-code \
		protocols/wlr-output-power-management-unstable-v1.xml $@

wlr-output-power-management-unstable-v1-protocol.o: wlr-output-power-management-unstable-v1-protocol.h

hyprland-toplevel-export-v1-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		subprojects/hyprland-protocols/protocols/hyprland-toplevel-export-v1.xml $@

hyprland-toplevel-export-v1-protocol.c:
	$(WAYLAND_SCANNER) private-code \
		subprojects/hyprland-protocols/protocols/hyprland-toplevel-export-v1.xml $@

hyprland-toplevel-export-v1-protocol.o: hyprland-toplevel-export-v1-protocol.h

hyprland-global-shortcuts-v1-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		subprojects/hyprland-protocols/protocols/hyprland-global-shortcuts-v1.xml $@

hyprland-global-shortcuts-v1-protocol.c:
	$(WAYLAND_SCANNER) private-code \
		subprojects/hyprland-protocols/protocols/hyprland-global-shortcuts-v1.xml $@

hyprland-global-shortcuts-v1-protocol.o: hyprland-global-shortcuts-v1-protocol.h

linux-dmabuf-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS)/unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml $@

linux-dmabuf-unstable-v1-protocol.c:
	$(WAYLAND_SCANNER) private-code \
		$(WAYLAND_PROTOCOLS)/unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml $@

linux-dmabuf-unstable-v1-protocol.o: linux-dmabuf-unstable-v1-protocol.h

wlr-foreign-toplevel-management-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		protocols/wlr-foreign-toplevel-management-unstable-v1.xml $@

wlr-foreign-toplevel-management-unstable-v1-protocol.c:
	$(WAYLAND_SCANNER) private-code \
		protocols/wlr-foreign-toplevel-management-unstable-v1.xml $@

wlr-foreign-toplevel-management-unstable-v1-protocol.o: wlr-foreign-toplevel-management-unstable-v1-protocol.h

fractional-scale-v1-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS)/staging/fractional-scale/fractional-scale-v1.xml $@

fractional-scale-v1-protocol.c:
	$(WAYLAND_SCANNER) private-code \
		$(WAYLAND_PROTOCOLS)/staging/fractional-scale/fractional-scale-v1.xml $@

fractional-scale-v1-protocol.o: fractional-scale-v1-protocol.h

text-input-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS)/unstable/text-input/text-input-unstable-v1.xml $@

text-input-unstable-v1-protocol.c:
	$(WAYLAND_SCANNER) private-code \
		$(WAYLAND_PROTOCOLS)/unstable/text-input/text-input-unstable-v1.xml $@

text-input-unstable-v1-protocol.o: text-input-unstable-v1-protocol.h

legacyrenderer:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -DLEGACY_RENDERER:BOOL=true -S . -B ./build -G Ninja
	cmake --build ./build --config Release --target all -j$(shell nproc)

legacyrendererdebug:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Debug -DLEGACY_RENDERER:BOOL=true -S . -B ./build -G Ninja
	cmake --build ./build --config Release --target all -j$(shell nproc)

release:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -S . -B ./build -G Ninja
	cmake --build ./build --config Release --target all -j$(shell nproc)

debug:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Debug -S . -B ./build -G Ninja
	cmake --build ./build --config Debug --target all -j$(shell nproc)

clear:
	rm -rf build
	rm -f *.o *-protocol.h *-protocol.c
	rm -f ./hyprctl/hyprctl
	rm -rf ./subprojects/wlroots/build

all:
	make clear
	make fixwlr
	cd ./subprojects/wlroots && meson setup build/ --buildtype=release && ninja -C build/ && cp ./build/libwlroots.so.12032 ${PREFIX}/lib/ || echo "Could not install libwlroots to ${PREFIX}/lib/libwlroots.so.12032"
	cd subprojects/udis86 && cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -S . -B./build -G Ninja && cmake --build ./build --config Release --target all -j$(shell nproc)
	make protocols
	make release
	make -C hyprctl all

install:
	make clear
	make fixwlr
	cd ./subprojects/wlroots && meson setup build/ --buildtype=release && ninja -C build/ && cp ./build/libwlroots.so.12032 ${PREFIX}/lib/ || echo "Could not install libwlroots to ${PREFIX}/lib/libwlroots.so.12032"
	cd subprojects/udis86 && cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -S . -B./build -G Ninja && cmake --build ./build --config Release --target all -j$(shell nproc) && cd ../..
	make protocols
	make release
	make -C hyprctl all

	mkdir -p ${PREFIX}/share/wayland-sessions
	mkdir -p ${PREFIX}/bin
	cp ./build/Hyprland ${PREFIX}/bin
	cp ./hyprctl/hyprctl ${PREFIX}/bin
	if [ ! -f ${PREFIX}/share/wayland-sessions/hyprland.desktop ]; then cp ./example/hyprland.desktop ${PREFIX}/share/wayland-sessions; fi
	mkdir -p ${PREFIX}/share/hyprland
	cp ./assets/wall_2K.png ${PREFIX}/share/hyprland
	cp ./assets/wall_4K.png ${PREFIX}/share/hyprland
	cp ./assets/wall_8K.png ${PREFIX}/share/hyprland

	install -Dm644 -t ${PREFIX}/share/man/man1 ./docs/*.1

cleaninstall:
	echo -en "make cleaninstall has been DEPRECATED, you should avoid using it in the future.\nRunning make install instead...\n"
	make install

uninstall:
	rm -f ${PREFIX}/share/wayland-sessions/hyprland.desktop
	rm -f ${PREFIX}/bin/Hyprland
	rm -f ${PREFIX}/bin/hyprctl
	rm -f ${PREFIX}/lib/libwlroots.so.12032
	rm -rf ${PREFIX}/share/hyprland
	rm -f ${PREFIX}/share/man/man1/Hyprland.1
	rm -f ${PREFIX}/share/man/man1/hyprctl.1

protocols: xdg-shell-protocol.o wlr-layer-shell-unstable-v1-protocol.o wlr-screencopy-unstable-v1-protocol.o idle-protocol.o ext-workspace-unstable-v1-protocol.o pointer-constraints-unstable-v1-protocol.o tablet-unstable-v2-protocol.o wlr-output-power-management-unstable-v1-protocol.o linux-dmabuf-unstable-v1-protocol.o hyprland-toplevel-export-v1-protocol.o wlr-foreign-toplevel-management-unstable-v1-protocol.o fractional-scale-v1-protocol.o text-input-unstable-v1-protocol.o hyprland-global-shortcuts-v1-protocol.o

fixwlr:
	sed -i -E 's/(soversion = 12)([^032]|$$)/soversion = 12032/g' subprojects/wlroots/meson.build

	rm -rf ./subprojects/wlroots/build

config:
	make protocols

	make fixwlr

	meson setup subprojects/wlroots/build subprojects/wlroots --prefix=${PREFIX} --buildtype=release -Dwerror=false -Dexamples=false
	ninja -C subprojects/wlroots/build/

	ninja -C subprojects/wlroots/build/ install

	cd subprojects/udis86 && cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -S . -B ./build -G Ninja && cmake --build ./build --config Release --target all -j$(shell nproc)

pluginenv:
	make protocols

	cd subprojects/udis86 && cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -S . -B ./build -G Ninja && cmake --build ./build --config Release --target all -j$(shell nproc)

	make fixwlr

	meson setup subprojects/wlroots/build subprojects/wlroots --prefix=${PREFIX} --buildtype=release -Dwerror=false -Dexamples=false
	ninja -C subprojects/wlroots/build/

configdebug:
	make protocols

	make fixwlr

	meson setup subprojects/wlroots/build subprojects/wlroots --prefix=${PREFIX} --buildtype=debug -Dwerror=false -Dexamples=false -Db_sanitize=address
	ninja -C subprojects/wlroots/build/

	ninja -C subprojects/wlroots/build/ install

man:
	pandoc ./docs/Hyprland.1.rst \
		--standalone \
		--variable=header:"Hyprland User Manual" \
		--variable=date:"${DATE}" \
		--variable=section:1 \
		--from rst \
		--to man > ./docs/Hyprland.1

	pandoc ./docs/hyprctl.1.rst \
		--standalone \
		--variable=header:"hyprctl User Manual" \
		--variable=date:"${DATE}" \
		--variable=section:1 \
		--from rst \
		--to man > ./docs/hyprctl.1
