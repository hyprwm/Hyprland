PREFIX = /usr/local

legacyrenderer:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -DLEGACY_RENDERER:BOOL=true -S . -B ./build -G Ninja
	cmake --build ./build --config Release --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`

legacyrendererdebug:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Debug -DLEGACY_RENDERER:BOOL=true -S . -B ./build -G Ninja
	cmake --build ./build --config Release --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`

release:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -S . -B ./build -G Ninja
	cmake --build ./build --config Release --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`

debug:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Debug -S . -B ./build -G Ninja
	cmake --build ./build --config Debug --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`

clear:
	rm -rf build
	rm -f ./protocols/*-protocol.h ./protocols/*-protocol.c
	rm -f ./hyprctl/hyprctl
	rm -rf ./subprojects/wlroots/build

all:
	$(MAKE) clear
	$(MAKE) fixwlr
	cd ./subprojects/wlroots && meson setup build/ --buildtype=release && ninja -C build/ && mkdir -p ${PREFIX}/lib/ && cp ./build/libwlroots.so.12032 ${PREFIX}/lib/ || echo "Could not install libwlroots to ${PREFIX}/lib/libwlroots.so.12032"
	cd subprojects/udis86 && cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -S . -B./build -G Ninja && cmake --build ./build --config Release --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`
	$(MAKE) release
	$(MAKE) -C hyprctl all

install:
	$(MAKE) clear
	$(MAKE) fixwlr
	cd ./subprojects/wlroots && meson setup build/ --buildtype=release && ninja -C build/ && mkdir -p ${PREFIX}/lib/ && cp ./build/libwlroots.so.12032 ${PREFIX}/lib/ || echo "Could not install libwlroots to ${PREFIX}/lib/libwlroots.so.12032"
	cd subprojects/udis86 && cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -S . -B./build -G Ninja && cmake --build ./build --config Release --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF` && cd ../..
	$(MAKE) release
	$(MAKE) -C hyprctl all

	mkdir -p ${PREFIX}/share/wayland-sessions
	mkdir -p ${PREFIX}/bin
	cp -f ./build/Hyprland ${PREFIX}/bin
	cp -f ./hyprctl/hyprctl ${PREFIX}/bin
	if [ ! -f ${PREFIX}/share/wayland-sessions/hyprland.desktop ]; then cp ./example/hyprland.desktop ${PREFIX}/share/wayland-sessions; fi
	mkdir -p ${PREFIX}/share/hyprland
	cp ./assets/wall_2K.png ${PREFIX}/share/hyprland
	cp ./assets/wall_4K.png ${PREFIX}/share/hyprland
	cp ./assets/wall_8K.png ${PREFIX}/share/hyprland

	mkdir -p ${PREFIX}/share/man/man1
	install -m644 ./docs/*.1 ${PREFIX}/share/man/man1

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

cleaninstall:
	echo -en "$(MAKE) cleaninstall has been DEPRECATED, you should avoid using it in the future.\nRunning $(MAKE) install instead...\n"
	$(MAKE) install

uninstall:
	rm -f ${PREFIX}/share/wayland-sessions/hyprland.desktop
	rm -f ${PREFIX}/bin/Hyprland
	rm -f ${PREFIX}/bin/hyprctl
	rm -f ${PREFIX}/lib/libwlroots.so.12032
	rm -rf ${PREFIX}/share/hyprland
	rm -f ${PREFIX}/share/man/man1/Hyprland.1
	rm -f ${PREFIX}/share/man/man1/hyprctl.1

fixwlr:
	sed -E -i -e 's/(soversion = 12)([^032]|$$)/soversion = 12032/g' subprojects/wlroots/meson.build

	rm -rf ./subprojects/wlroots/build

config:
	$(MAKE) fixwlr

	meson setup subprojects/wlroots/build subprojects/wlroots --prefix=${PREFIX} --buildtype=release -Dwerror=false -Dexamples=false
	ninja -C subprojects/wlroots/build/

	ninja -C subprojects/wlroots/build/ install

	cd subprojects/udis86 && cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -S . -B ./build -G Ninja && cmake --build ./build --config Release --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`

pluginenv:
	cd subprojects/udis86 && cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -S . -B ./build -G Ninja && cmake --build ./build --config Release --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`

	$(MAKE) fixwlr

	meson setup subprojects/wlroots/build subprojects/wlroots --prefix=${PREFIX} --buildtype=release -Dwerror=false -Dexamples=false
	ninja -C subprojects/wlroots/build/

	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -S . -B ./build -G Ninja

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

configdebug:
	$(MAKE) fixwlr

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
