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

nopch:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_INSTALL_PREFIX:STRING=${PREFIX} -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON -S . -B ./build -G Ninja
	cmake --build ./build --config Release --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`

clear:
	rm -rf build
	rm -f ./protocols/*-protocol.h ./protocols/*-protocol.c
	rm -rf ./subprojects/wlroots-hyprland/build

all:
	@if [[ "$EUID" = 0 ]]; then echo -en "Avoid running $(MAKE) all as sudo.\n"; fi
	$(MAKE) clear
	$(MAKE) release

install:
	@if [ ! -f ./build/Hyprland ]; then echo -en "You need to run $(MAKE) all first.\n" && exit 1; fi
	@echo -en "!NOTE: Please note make install does not compile Hyprland and only installs the already built files."

	mkdir -p ${PREFIX}/share/wayland-sessions
	mkdir -p ${PREFIX}/bin
	mkdir -p ${PREFIX}/share/hyprland
	mkdir -p ${PREFIX}/share/bash-completion/completions
	mkdir -p ${PREFIX}/share/fish/vendor_completions.d
	mkdir -p ${PREFIX}/share/zsh/site-functions
	cp -f ./build/Hyprland ${PREFIX}/bin
	cp -f ./build/hyprctl/hyprctl ${PREFIX}/bin
	cp -f ./build/hyprpm/hyprpm ${PREFIX}/bin
	cp -f ./hyprctl/hyprctl.bash ${PREFIX}/share/bash-completion/completions/hyprctl
	cp -f ./hyprctl/hyprctl.fish ${PREFIX}/share/fish/vendor_completions.d/hyprctl.fish
	cp -f ./hyprctl/hyprctl.zsh ${PREFIX}/share/zsh/site-functions/_hyprctl
	cp -f ./hyprpm/hyprpm.bash ${PREFIX}/share/bash-completion/completions/hyprpm
	cp -f ./hyprpm/hyprpm.fish ${PREFIX}/share/fish/vendor_completions.d/hyprpm.fish
	cp -f ./hyprpm/hyprpm.zsh ${PREFIX}/share/zsh/site-functions/_hyprpm
	chmod 755 ${PREFIX}/bin/Hyprland
	chmod 755 ${PREFIX}/bin/hyprctl
	chmod 755 ${PREFIX}/bin/hyprpm
	cd ${PREFIX}/bin && ln -sf Hyprland hyprland
	if [ ! -f ${PREFIX}/share/wayland-sessions/hyprland.desktop ]; then cp ./example/hyprland.desktop ${PREFIX}/share/wayland-sessions; fi
	cp ./assets/wall* ${PREFIX}/share/hyprland
	mkdir -p ${PREFIX}/share/xdg-desktop-portal
	cp ./assets/hyprland-portals.conf ${PREFIX}/share/xdg-desktop-portal

	mkdir -p ${PREFIX}/share/man/man1
	install -m644 ./docs/*.1 ${PREFIX}/share/man/man1

	$(MAKE) installheaders

uninstall:
	rm -f ${PREFIX}/share/wayland-sessions/hyprland.desktop
	rm -f ${PREFIX}/bin/Hyprland
	rm -f ${PREFIX}/bin/hyprland
	rm -f ${PREFIX}/bin/hyprctl
	rm -f ${PREFIX}/bin/hyprpm
	rm -rf ${PREFIX}/share/hyprland
	rm -f ${PREFIX}/share/man/man1/Hyprland.1
	rm -f ${PREFIX}/share/man/man1/hyprctl.1
	rm -f ${PREFIX}/share/bash-completion/completions/hyprctl
	rm -f ${PREFIX}/share/fish/vendor_completions.d/hyprctl.fish
	rm -f ${PREFIX}/share/zsh/site-functions/_hyprctl
	rm -f ${PREFIX}/share/bash-completion/completions/hyprpm
	rm -f ${PREFIX}/share/fish/vendor_completions.d/hyprpm.fish
	rm -f ${PREFIX}/share/zsh/site-functions/_hyprpm

pluginenv:
	@echo -en "$(MAKE) pluginenv has been deprecated.\nPlease run $(MAKE) all && sudo $(MAKE) installheaders\n"
	@exit 1
	
installheaders:
	@if [ ! -f ./src/version.h ]; then echo -en "You need to run $(MAKE) all first.\n" && exit 1; fi

	rm -fr ${PREFIX}/include/hyprland
	mkdir -p ${PREFIX}/include/hyprland
	mkdir -p ${PREFIX}/include/hyprland/protocols
	mkdir -p ${PREFIX}/include/hyprland/wlroots-hyprland
	mkdir -p ${PREFIX}/share/pkgconfig
	
	find src -name '*.h*' -print0 | cpio --quiet -0dump ${PREFIX}/include/hyprland
	cd subprojects/wlroots-hyprland/include && find . -name '*.h*' -print0 | cpio --quiet -0dump ${PREFIX}/include/hyprland/wlroots-hyprland && cd ../../..
	cd subprojects/wlroots-hyprland/build/include && find . -name '*.h*' -print0 | cpio --quiet -0dump ${PREFIX}/include/hyprland/wlroots-hyprland && cd ../../../..
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

asan:
	@echo -en "!!WARNING!!\nOnly run this in the TTY.\n"
	@pidof Hyprland > /dev/null && echo -ne "Refusing to run with Hyprland running.\n" || echo ""
	@pidof Hyprland > /dev/null && exit 1 || echo ""

	rm -rf ./wayland
	git reset --hard

	@echo -en "If you want to apply a patch, input its path (leave empty for none):\n"
	@read patchvar
	@if [-n "$patchvar"]; then patch -p1 < $patchvar || echo ""; else echo "No patch specified"; fi

	git clone --recursive https://gitlab.freedesktop.org/wayland/wayland
	cd wayland && patch -p1 < ../scripts/waylandStatic.diff && meson setup build --buildtype=debug -Db_sanitize=address -Ddocumentation=false && ninja -C build && cd ..
	cp ./wayland/build/src/libwayland-server.a .
	@echo "Wayland done"

	patch -p1 < ./scripts/hyprlandStaticAsan.diff
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Debug -DWITH_ASAN:STRING=True -DUSE_TRACY:STRING=False -DUSE_TRACY_GPU:STRING=False -S . -B ./build -G Ninja
	cmake --build ./build --config Debug --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`
	@echo "Hyprland done"

	ASAN_OPTIONS="detect_odr_violation=0,log_path=asan.log" HYPRLAND_NO_CRASHREPORTER=1 ./build/Hyprland -c ~/.config/hypr/hyprland.conf
	
