PREFIX = /usr/local

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
	rm -f ./protocols/*-protocol.h ./protocols/*-protocol.c
	rm -f ./hyprctl/hyprctl
	rm -rf ./subprojects/wlroots/build

all:
	make clear
	make fixwlr
	cd ./subprojects/wlroots && meson setup build/ --buildtype=release && ninja -C build/ && cp ./build/libwlroots.so.12032 ${PREFIX}/lib/ || echo "Could not install libwlroots to ${PREFIX}/lib/libwlroots.so.12032"
	cd subprojects/udis86 && cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -S . -B./build -G Ninja && cmake --build ./build --config Release --target all -j$(shell nproc)
	make release
	make -C hyprctl all

install:
	make clear
	make fixwlr
	cd ./subprojects/wlroots && meson setup build/ --buildtype=release && ninja -C build/ && cp ./build/libwlroots.so.12032 ${PREFIX}/lib/ || echo "Could not install libwlroots to ${PREFIX}/lib/libwlroots.so.12032"
	cd subprojects/udis86 && cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -S . -B./build -G Ninja && cmake --build ./build --config Release --target all -j$(shell nproc) && cd ../..
	make release
	make -C hyprctl all

	mkdir -p ${PREFIX}/share/wayland-sessions
	mkdir -p ${PREFIX}/bin
	cp ./build/Hyprland ${PREFIX}/bin -f
	cp ./hyprctl/hyprctl ${PREFIX}/bin -f
	if [ ! -f ${PREFIX}/share/wayland-sessions/hyprland.desktop ]; then cp ./example/hyprland.desktop ${PREFIX}/share/wayland-sessions; fi
	mkdir -p ${PREFIX}/share/hyprland
	cp ./assets/wall_2K.png ${PREFIX}/share/hyprland
	cp ./assets/wall_4K.png ${PREFIX}/share/hyprland
	cp ./assets/wall_8K.png ${PREFIX}/share/hyprland

	install -Dm644 -t ${PREFIX}/share/man/man1 ./docs/*.1
	mkdir -p ${PREFIX}/include/hyprland

	mkdir -p ${PREFIX}/include/hyprland/protocols
	mkdir -p ${PREFIX}/share/pkgconfig
	find src -name '*.h*' -exec cp --parents '{}' ${PREFIX}/include/hyprland ';'
	cp ./protocols/*-protocol.h ${PREFIX}/include/hyprland/protocols
	cp ./build/hyprland.pc ${PREFIX}/share/pkgconfig

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

fixwlr:
	sed -i -E 's/(soversion = 12)([^032]|$$)/soversion = 12032/g' subprojects/wlroots/meson.build

	rm -rf ./subprojects/wlroots/build

config:
	make fixwlr

	meson setup subprojects/wlroots/build subprojects/wlroots --prefix=${PREFIX} --buildtype=release -Dwerror=false -Dexamples=false
	ninja -C subprojects/wlroots/build/

	ninja -C subprojects/wlroots/build/ install

	cd subprojects/udis86 && cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -S . -B ./build -G Ninja && cmake --build ./build --config Release --target all -j$(shell nproc)

pluginenv:
	cd subprojects/udis86 && cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -S . -B ./build -G Ninja && cmake --build ./build --config Release --target all -j$(shell nproc)

	make fixwlr

	meson setup subprojects/wlroots/build subprojects/wlroots --prefix=${PREFIX} --buildtype=release -Dwerror=false -Dexamples=false
	ninja -C subprojects/wlroots/build/

	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -S . -B ./build -G Ninja

configdebug:
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
