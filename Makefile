PREFIX = /usr/local

stub:
	@echo "Do not run $(MAKE) directly without any arguments. Please refer to the wiki on how to compile Hyprland."

release:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_INSTALL_PREFIX:STRING=${PREFIX} -S . -B ./build
	cmake --build ./build --config Release --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`

debug:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Debug -DTESTS=true -DCMAKE_INSTALL_PREFIX:STRING=${PREFIX} -S . -B ./build
	cmake --build ./build --config Debug --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`

nopch:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_INSTALL_PREFIX:STRING=${PREFIX} -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON -S . -B ./build
	cmake --build ./build --config Release --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`

clear:
	rm -rf build
	rm -f ./protocols/*.h ./protocols/*.c ./protocols/*.cpp ./protocols/*.hpp

all:
	$(MAKE) clear
	$(MAKE) release

install:
	cmake --install ./build

uninstall:
	xargs rm < ./build/install_manifest.txt

pluginenv:
	@echo -en "$(MAKE) pluginenv has been deprecated.\nPlease run $(MAKE) all && sudo $(MAKE) installheaders\n"
	@exit 1

installheaders:
	@if [ ! -f ./src/version.h ]; then echo -en "You need to run $(MAKE) all first.\n" && exit 1; fi

	# remove previous headers from hyprpm's dir
	rm -fr ${PREFIX}/include/hyprland
	mkdir -p ${PREFIX}/include/hyprland
	mkdir -p ${PREFIX}/include/hyprland/protocols
	mkdir -p ${PREFIX}/share/pkgconfig

	cmake --build ./build --config Release --target generate-protocol-headers

	find src -type f \( -name '*.hpp' -o -name '*.h' -o -name '*.inc' \) -print0 | cpio --quiet -0dump ${PREFIX}/include/hyprland
	cp ./protocols/*.h* ${PREFIX}/include/hyprland/protocols
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

asan:
	@echo -en "!!WARNING!!\nOnly run this in the TTY.\n"
	@pidof Hyprland > /dev/null && echo -ne "Refusing to run with Hyprland running.\n" || echo ""
	@pidof Hyprland > /dev/null && exit 1 || echo ""

	rm -rf ./wayland
	#git reset --hard

	@echo -en "If you want to apply a patch, input its path (leave empty for none):\n"
	@read patchvar; \
	 if [ -n "$$patchvar" ]; then patch -p1 < "$$patchvar" || echo ""; else echo "No patch specified"; fi

	git clone --recursive https://gitlab.freedesktop.org/wayland/wayland
	cd wayland && patch -p1 < ../scripts/waylandStatic.diff && meson setup build --buildtype=debug -Db_sanitize=address -Ddocumentation=false && ninja -C build && cd ..
	cp ./wayland/build/src/libwayland-server.a .
	@echo "Wayland done"

	patch -p1 < ./scripts/hyprlandStaticAsan.diff
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Debug -DWITH_ASAN:STRING=True -DUSE_TRACY:STRING=False -DUSE_TRACY_GPU:STRING=False -S . -B ./build -G Ninja
	cmake --build ./build --config Debug --target all
	@echo "Hyprland done"

	ASAN_OPTIONS="detect_odr_violation=0,log_path=asan.log" HYPRLAND_NO_CRASHREPORTER=1 ./build/Hyprland -c ~/.config/hypr/hyprland.conf

test:
	$(MAKE) debug
	./build/hyprtester/hyprtester -c hyprtester/test.conf -b ./build/Hyprland -p hyprtester/plugin/hyprtestplugin.so
