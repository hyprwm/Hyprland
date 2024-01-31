PREFIX = /usr/local

legacyrenderer:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_INSTALL_PREFIX:STRING=${PREFIX} -DLEGACY_RENDERER:BOOL=true -S . -B ./build -G Ninja
	cmake --build ./build --config Release --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`
	chmod -R 777 ./build

legacyrendererdebug:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_INSTALL_PREFIX:STRING=${PREFIX} -DLEGACY_RENDERER:BOOL=true -S . -B ./build -G Ninja
	cmake --build ./build --config Release --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`
	chmod -R 777 ./build

release:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_INSTALL_PREFIX:STRING=${PREFIX} -S . -B ./build -G Ninja
	cmake --build ./build --config Release --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`
	chmod -R 777 ./build

debug:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_INSTALL_PREFIX:STRING=${PREFIX} -S . -B ./build -G Ninja
	cmake --build ./build --config Debug --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`
	chmod -R 777 ./build

clear:
	rm -rf build
	rm -f ./protocols/*-protocol.h ./protocols/*-protocol.c
	rm -rf ./subprojects/wlroots/build

all:
	@if [[ "$EUID" = 0 ]]; then echo -en "Avoid running $(MAKE) all as sudo.\n"; fi
	$(MAKE) clear
	$(MAKE) release

install:
	@if [ ! -f ./build/Hyprland ]; then echo -en "You need to run $(MAKE) all first.\n" && exit 1; fi
	@echo -en "!NOTE: Please note make install does not compile Hyprland and only installs the already built files."

	mkdir -p ${PREFIX}/share/wayland-sessions
	mkdir -p ${PREFIX}/bin
	cp -f ./build/Hyprland ${PREFIX}/bin
	cp -f ./build/hyprctl/hyprctl ${PREFIX}/bin
	cp -f ./build/hyprpm/hyprpm ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/Hyprland
	chmod 755 ${PREFIX}/bin/hyprctl
	chmod 755 ${PREFIX}/bin/hyprpm
	cd ${PREFIX}/bin && ln -sf Hyprland hyprland
	if [ ! -f ${PREFIX}/share/wayland-sessions/hyprland.desktop ]; then cp ./example/hyprland.desktop ${PREFIX}/share/wayland-sessions; fi
	mkdir -p ${PREFIX}/share/hyprland
	cp ./assets/wall* ${PREFIX}/share/hyprland
	mkdir -p ${PREFIX}/share/xdg-desktop-portal
	cp ./assets/hyprland-portals.conf ${PREFIX}/share/xdg-desktop-portal

	mkdir -p ${PREFIX}/share/man/man1
	install -m644 ./docs/*.1 ${PREFIX}/share/man/man1

	mkdir -p ${PREFIX}/lib/
	cp ./subprojects/wlroots/build/libwlroots.so.13032 ${PREFIX}/lib/

	$(MAKE) installheaders

uninstall:
	rm -f ${PREFIX}/share/wayland-sessions/hyprland.desktop
	rm -f ${PREFIX}/bin/Hyprland
	rm -f ${PREFIX}/bin/hyprland
	rm -f ${PREFIX}/bin/hyprctl
	rm -f ${PREFIX}/bin/hyprpm
	rm -f ${PREFIX}/lib/libwlroots.so.13032
	rm -rf ${PREFIX}/share/hyprland
	rm -f ${PREFIX}/share/man/man1/Hyprland.1
	rm -f ${PREFIX}/share/man/man1/hyprctl.1

pluginenv:
	@echo -en "$(MAKE) pluginenv has been deprecated.\nPlease run $(MAKE) all && sudo $(MAKE) installheaders\n"
	@exit 1
	
installheaders:
	@if [ ! -f ./src/version.h ]; then echo -en "You need to run $(MAKE) all first.\n" && exit 1; fi

	mkdir -p ${PREFIX}/include/hyprland
	mkdir -p ${PREFIX}/include/hyprland/protocols
	mkdir -p ${PREFIX}/include/hyprland/wlroots
	mkdir -p ${PREFIX}/share/pkgconfig
	
	find src -name '*.h*' -print0 | cpio --quiet -0dump ${PREFIX}/include/hyprland
	cd subprojects/wlroots/include && find . -name '*.h*' -print0 | cpio --quiet -0dump ${PREFIX}/include/hyprland/wlroots && cd ../../..
	cd subprojects/wlroots/build/include && find . -name '*.h*' -print0 | cpio --quiet -0dump ${PREFIX}/include/hyprland/wlroots && cd ../../../..
	cp ./protocols/*-protocol.h ${PREFIX}/include/hyprland/protocols
	cp ./build/hyprland.pc ${PREFIX}/share/pkgconfig
	if [ -d /usr/share/pkgconfig ]; then cp ./build/hyprland.pc /usr/share/pkgconfig 2>/dev/null || true; fi

	chmod -R 755 ${PREFIX}/include/hyprland
	chmod 755 ${PREFIX}/share/pkgconfig

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
