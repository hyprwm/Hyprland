PREFIX ?= /usr/local
BUILDDIR ?= ./build
CMAKE_BUILD_TYPE := Release
CMAKE_ARGS = -DCMAKE_BUILD_TYPE:STRING=$(CMAKE_BUILD_TYPE) -DCMAKE_INSTALL_PREFIX:STRING=$(PREFIX)
CMAKE_BUILDTYPE_FILE = $(BUILDDIR)/cmake_last_build_type
CMAKE_GENERATE_CMD = cmake -Wno-unused-cli $(CMAKE_ARGS) -S . -B $(BUILDDIR)

.DEFAULT: stub
.PHONY: stub cmake_smartbuild release debug nopch clear all install uninstall pluginenv installheaders man asan format-check format-fix test

stub:
	@echo "Do not run $(MAKE) directly without any arguments. Please refer to the wiki on how to compile Hyprland."

# Regenerate when CMakeLists have changed
$(CMAKE_BUILDTYPE_FILE): $(shell find -type f -name CMakeLists.txt -not -path '*/_deps/*')
	mkdir -p "$(BUILDDIR)"
	echo "$(CMAKE_BUILD_TYPE)+$(CMAKE_ARGS)" > "$(CMAKE_BUILDTYPE_FILE)"
	$(CMAKE_GENERATE_CMD)

# Regenerate when CMake options have changed, then build
cmake_smartbuild: $(CMAKE_BUILDTYPE_FILE)
	read lastbuild < "$(CMAKE_BUILDTYPE_FILE)" && test "$$lastbuild" = "$(CMAKE_BUILD_TYPE)+$(CMAKE_ARGS)" || { \
		echo "$(CMAKE_BUILD_TYPE)+$(CMAKE_ARGS)" > "$(CMAKE_BUILDTYPE_FILE)"; \
		$(CMAKE_GENERATE_CMD); \
	}
	cmake --build $(BUILDDIR) --config $(CMAKE_BUILD_TYPE) --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`

release: cmake_smartbuild

debug: CMAKE_BUILD_TYPE := Debug
debug: CMAKE_ARGS += -DTESTS=true
debug: cmake_smartbuild

nopch: CMAKE_ARGS += -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON
nopch: cmake_smartbuild

clear:
	rm -rf build
	rm -f ./protocols/*.h ./protocols/*.c ./protocols/*.cpp ./protocols/*.hpp
	rm -f ./hyprctl/hw-protocols/*.cpp ./hyprctl/hw-protocols/*.hpp

all:
	$(MAKE) clear
	$(MAKE) release

install: cmake_smartbuild
	cmake --install $(BUILDDIR)

uninstall:
	xargs rm < $(BUILDDIR)/install_manifest.txt

pluginenv:
	@echo -en "$(MAKE) pluginenv has been deprecated.\nPlease run $(MAKE) all && sudo $(MAKE) installheaders\n"
	@exit 1

installheaders: cmake_smartbuild
	@if [ ! -f ./src/version.h ]; then echo -en "You need to run $(MAKE) all first.\n" && exit 1; fi

	# remove previous headers from hyprpm's dir
	rm -fr ${PREFIX}/include/hyprland
	mkdir -p ${PREFIX}/include/hyprland
	mkdir -p ${PREFIX}/include/hyprland/protocols
	mkdir -p ${PREFIX}/share/pkgconfig

	cmake --build $(BUILDDIR) --config $(CMAKE_BUILD_TYPE) --target generate-protocol-headers

	find src -type f \( -name '*.hpp' -o -name '*.h' -o -name '*.inc' \) -print0 | cpio --quiet -0dump ${PREFIX}/include/hyprland
	cp ./protocols/*.h* ${PREFIX}/include/hyprland/protocols
	cp $(BUILDDIR)/hyprland.pc ${PREFIX}/share/pkgconfig
	if [ -d /usr/share/pkgconfig ]; then cp $(BUILDDIR)/hyprland.pc /usr/share/pkgconfig 2>/dev/null || true; fi

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

asan: CMAKE_BUILD_TYPE := Debug
asan: cmake_smartbuild
	@echo -en "!!WARNING!!\nOnly run this in the TTY.\n"
	@if pidof Hyprland > /dev/null; then echo -ne "Refusing to run with Hyprland running.\n"; exit 1; fi

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
	$(CMAKE_GENERATE_CMD) -DWITH_ASAN:STRING=True -DUSE_TRACY:STRING=False -DUSE_TRACY_GPU:STRING=False
	cmake --build $(BUILDDIR) --config $(CMAKE_BUILD_TYPE) --target all
	@echo "Hyprland done"

	ASAN_OPTIONS="detect_odr_violation=0,log_path=asan.log" HYPRLAND_NO_CRASHREPORTER=1 $(BUILDDIR)/Hyprland -c ~/.config/hypr/hyprland.lua

format-check:
	@find src hyprctl hyprpm start tests hyprtester -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \) \
		! -path "src/render/shaders/Shaders.hpp" \
		! -path "hyprctl/hw-protocols/*" \
		! -path "hyprtester/protocols/*" \
		| xargs clang-format --dry-run --Werror

format-fix:
	@find src hyprctl hyprpm start tests hyprtester -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \) \
		! -path "src/render/shaders/Shaders.hpp" \
		! -path "hyprctl/hw-protocols/*" \
		! -path "hyprtester/protocols/*" \
		| xargs clang-format -i

test: debug
	$(BUILDDIR)/hyprtester/hyprtester -c hyprtester/test.lua -b $(BUILDDIR)/Hyprland -p hyprtester/plugin/hyprtestplugin.so $(TESTS)
