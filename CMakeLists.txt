cmake_minimum_required(VERSION 3.30)

# Get version
file(READ "${CMAKE_SOURCE_DIR}/VERSION" VER_RAW)
string(STRIP ${VER_RAW} VER)

project(
  Hyprland
  DESCRIPTION "A Modern C++ Wayland Compositor"
  VERSION ${VER})

include(CTest)
include(CheckIncludeFile)
include(GNUInstallDirs)

set(HYPRLAND_VERSION ${VER})
set(PREFIX ${CMAKE_INSTALL_PREFIX})
set(INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR})
set(BINDIR ${CMAKE_INSTALL_BINDIR})
configure_file(hyprland.pc.in hyprland.pc @ONLY)

set(CMAKE_MESSAGE_LOG_LEVEL "STATUS")

message(STATUS "Gathering git info")

# Get git info hash and branch
execute_process(COMMAND ./scripts/generateVersion.sh
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
# Make shader files includable
execute_process(COMMAND ./scripts/generateShaderIncludes.sh
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

find_package(PkgConfig REQUIRED)

# Try to find canihavesomecoffee's udis86 using pkgconfig vmd/udis86 does not
# provide a .pc file and won't be detected this way
pkg_check_modules(udis_dep IMPORTED_TARGET udis86>=1.7.2)

# Fallback to subproject
if(NOT udis_dep_FOUND)
  add_subdirectory("subprojects/udis86")
  include_directories("subprojects/udis86")
  message(STATUS "udis86 dependency not found, falling back to subproject")
endif()

if(CMAKE_BUILD_TYPE)
  string(TOLOWER ${CMAKE_BUILD_TYPE} BUILDTYPE_LOWER)
  if(BUILDTYPE_LOWER STREQUAL "release")
    # Pass.
  elseif(BUILDTYPE_LOWER STREQUAL "debug")
    # Pass.
  elseif(BUILDTYPE_LOWER STREQUAL "relwithdebinfo")
    set(BUILDTYPE_LOWER "debugoptimized")
  elseif(BUILDTYPE_LOWER STREQUAL "minsizerel")
    set(BUILDTYPE_LOWER "minsize")
  elseif(BUILDTYPE_LOWER STREQUAL "none")
    set(BUILDTYPE_LOWER "plain")
  else()
    set(BUILDTYPE_LOWER "release")
  endif()
else()
  set(BUILDTYPE_LOWER "release")
endif()

pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
message(STATUS "Found wayland-protocols at ${WAYLAND_PROTOCOLS_DIR}")
pkg_get_variable(WAYLAND_SCANNER_PKGDATA_DIR wayland-scanner pkgdatadir)
message(
  STATUS "Found wayland-scanner pkgdatadir at ${WAYLAND_SCANNER_PKGDATA_DIR}")

if(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)
  message(STATUS "Configuring Hyprland in Debug with CMake")
  add_compile_definitions(HYPRLAND_DEBUG)
else()
  add_compile_options(-O3)
  message(STATUS "Configuring Hyprland in Release with CMake")
endif()

add_compile_definitions(HYPRLAND_VERSION="${HYPRLAND_VERSION}")

include_directories(. "src/" "protocols/")

set(CMAKE_CXX_STANDARD 26)
set(CXX_STANDARD_REQUIRED ON)
add_compile_options(
  -Wall
  -Wextra
  -Wpedantic
  -Wno-unused-parameter
  -Wno-unused-value
  -Wno-missing-field-initializers
  -Wno-gnu-zero-variadic-macro-arguments
  -Wno-narrowing
  -Wno-pointer-arith
  -Wno-clobbered
  -fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=)

set(CMAKE_EXECUTABLE_ENABLE_EXPORTS TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

message(STATUS "Checking deps...")

find_package(Threads REQUIRED)

set(GLES_VERSION "GLES3")
find_package(OpenGL REQUIRED COMPONENTS ${GLES_VERSION})

pkg_check_modules(aquamarine_dep REQUIRED IMPORTED_TARGET aquamarine>=0.9.3)
pkg_check_modules(hyprlang_dep REQUIRED IMPORTED_TARGET hyprlang>=0.3.2)
pkg_check_modules(hyprcursor_dep REQUIRED IMPORTED_TARGET hyprcursor>=0.1.7)
pkg_check_modules(hyprutils_dep REQUIRED IMPORTED_TARGET hyprutils>=0.8.2)
pkg_check_modules(hyprgraphics_dep REQUIRED IMPORTED_TARGET hyprgraphics>=0.1.6)

string(REPLACE "." ";" AQ_VERSION_LIST ${aquamarine_dep_VERSION})
list(GET AQ_VERSION_LIST 0 AQ_VERSION_MAJOR)
list(GET AQ_VERSION_LIST 1 AQ_VERSION_MINOR)
list(GET AQ_VERSION_LIST 2 AQ_VERSION_PATCH)

add_compile_definitions(AQUAMARINE_VERSION="${aquamarine_dep_VERSION}")
add_compile_definitions(AQUAMARINE_VERSION_MAJOR=${AQ_VERSION_MAJOR})
add_compile_definitions(AQUAMARINE_VERSION_MINOR=${AQ_VERSION_MINOR})
add_compile_definitions(AQUAMARINE_VERSION_PATCH=${AQ_VERSION_PATCH})
add_compile_definitions(HYPRLANG_VERSION="${hyprlang_dep_VERSION}")
add_compile_definitions(HYPRUTILS_VERSION="${hyprutils_dep_VERSION}")
add_compile_definitions(HYPRCURSOR_VERSION="${hyprcursor_dep_VERSION}")
add_compile_definitions(HYPRGRAPHICS_VERSION="${hyprgraphics_dep_VERSION}")

pkg_check_modules(
  deps
  REQUIRED
  IMPORTED_TARGET
  xkbcommon
  uuid
  wayland-server>=1.22.90
  wayland-protocols>=1.45
  cairo
  pango
  pangocairo
  pixman-1
  xcursor
  libdrm
  libinput>=1.28
  gbm
  gio-2.0
  re2)

find_package(hyprwayland-scanner 0.3.10 REQUIRED)

file(GLOB_RECURSE SRCFILES "src/*.cpp")

set(TRACY_CPP_FILES "")
if(USE_TRACY)
  set(TRACY_CPP_FILES "subprojects/tracy/public/TracyClient.cpp")
  message(STATUS "Tracy enabled, TRACY_CPP_FILES: " ${TRACY_CPP_FILES})
endif()

add_executable(Hyprland ${SRCFILES} ${TRACY_CPP_FILES})

set(USE_GPROF OFF)

if(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)
  message(STATUS "Setting debug flags")

  if(WITH_ASAN)
    message(STATUS "Enabling ASan")

    target_link_libraries(Hyprland asan)
    target_compile_options(Hyprland PUBLIC -fsanitize=address)
  endif()

  if(USE_TRACY)
    message(STATUS "Tracy is turned on")

    option(TRACY_ENABLE "" ON)
    option(TRACY_ON_DEMAND "" ON)
    add_subdirectory(subprojects/tracy)

    target_link_libraries(Hyprland Tracy::TracyClient)

    if(USE_TRACY_GPU)
      message(STATUS "Tracy GPU Profiling is turned on")
      add_compile_definitions(USE_TRACY_GPU)
    endif()
  endif()

  add_compile_options(-fno-pie -fno-builtin)
  add_link_options(-no-pie -fno-builtin)
  if(USE_GPROF)
    add_compile_options(-pg)
    add_link_options(-pg)
  endif()
endif()

check_include_file("execinfo.h" EXECINFOH)
if(EXECINFOH)
  message(STATUS "Configuration supports execinfo")
  add_compile_definitions(HAS_EXECINFO)
endif()

include(CheckLibraryExists)
check_library_exists(execinfo backtrace "" HAVE_LIBEXECINFO)
if(HAVE_LIBEXECINFO)
  target_link_libraries(Hyprland execinfo)
endif()

check_include_file("sys/timerfd.h" HAS_TIMERFD)
pkg_check_modules(epoll IMPORTED_TARGET epoll-shim)
if(NOT HAS_TIMERFD AND epoll_FOUND)
  target_link_libraries(Hyprland PkgConfig::epoll)
endif()

check_include_file("sys/inotify.h" HAS_INOTIFY)
pkg_check_modules(inotify IMPORTED_TARGET libinotify)
if(NOT HAS_INOTIFY AND inotify_FOUND)
  target_link_libraries(Hyprland PkgConfig::inotify)
endif()

if(NO_XWAYLAND)
  message(STATUS "Using the NO_XWAYLAND flag, disabling XWayland!")
  add_compile_definitions(NO_XWAYLAND)
else()
  message(STATUS "XWAYLAND Enabled (NO_XWAYLAND not defined) checking deps...")
  pkg_check_modules(
    xdeps
    REQUIRED
    IMPORTED_TARGET
    xcb
    xcb-render
    xcb-xfixes
    xcb-icccm
    xcb-composite
    xcb-res
    xcb-errors)
  target_link_libraries(Hyprland PkgConfig::xdeps)
endif()

if(NO_SYSTEMD)
  message(STATUS "SYSTEMD support is disabled...")
else()
  message(STATUS "SYSTEMD support is requested (NO_SYSTEMD not defined)...")
  add_compile_definitions(USES_SYSTEMD)

  # session file -uwsm
  if(NO_UWSM)
    message(STATUS "UWSM support is disabled...")
  else()
    message(STATUS "UWSM support is enabled (NO_UWSM not defined)...")
    install(FILES ${CMAKE_SOURCE_DIR}/systemd/hyprland-uwsm.desktop
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/wayland-sessions)
  endif()
endif()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

message(STATUS "Setting precompiled headers")

target_precompile_headers(Hyprland PRIVATE
                          $<$<COMPILE_LANGUAGE:CXX>:src/pch/pch.hpp>)

message(STATUS "Setting link libraries")

target_link_libraries(
  Hyprland
  rt
  PkgConfig::aquamarine_dep
  PkgConfig::hyprlang_dep
  PkgConfig::hyprutils_dep
  PkgConfig::hyprcursor_dep
  PkgConfig::hyprgraphics_dep
  PkgConfig::deps)
if(udis_dep_FOUND)
  target_link_libraries(Hyprland PkgConfig::udis_dep)
else()
  target_link_libraries(Hyprland libudis86)
endif()

# used by `make installheaders`, to ensure the headers are generated
add_custom_target(generate-protocol-headers)

function(protocolnew protoPath protoName external)
  if(external)
    set(path ${protoPath})
  else()
    set(path ${WAYLAND_PROTOCOLS_DIR}/${protoPath})
  endif()
  add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/protocols/${protoName}.cpp
           ${CMAKE_SOURCE_DIR}/protocols/${protoName}.hpp
    COMMAND hyprwayland-scanner ${path}/${protoName}.xml
            ${CMAKE_SOURCE_DIR}/protocols/
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  target_sources(Hyprland PRIVATE protocols/${protoName}.cpp
                                  protocols/${protoName}.hpp)
  target_sources(generate-protocol-headers
                 PRIVATE ${CMAKE_SOURCE_DIR}/protocols/${protoName}.hpp)
endfunction()
function(protocolWayland)
  add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/protocols/wayland.cpp
           ${CMAKE_SOURCE_DIR}/protocols/wayland.hpp
    COMMAND
      hyprwayland-scanner --wayland-enums
      ${WAYLAND_SCANNER_PKGDATA_DIR}/wayland.xml ${CMAKE_SOURCE_DIR}/protocols/
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  target_sources(Hyprland PRIVATE protocols/wayland.cpp protocols/wayland.hpp)
  target_sources(generate-protocol-headers
                 PRIVATE ${CMAKE_SOURCE_DIR}/protocols/wayland.hpp)
endfunction()

target_link_libraries(Hyprland OpenGL::EGL OpenGL::GL Threads::Threads)

pkg_check_modules(hyprland_protocols_dep hyprland-protocols>=0.6.4)
if(hyprland_protocols_dep_FOUND)
  pkg_get_variable(HYPRLAND_PROTOCOLS hyprland-protocols pkgdatadir)
  message(STATUS "hyprland-protocols dependency set to ${HYPRLAND_PROTOCOLS}")
else()
  set(HYPRLAND_PROTOCOLS "subprojects/hyprland-protocols")
  message(STATUS "hyprland-protocols subproject set to ${HYPRLAND_PROTOCOLS}")
endif()

protocolnew("${HYPRLAND_PROTOCOLS}/protocols" "hyprland-global-shortcuts-v1"
            true)
protocolnew("unstable/text-input" "text-input-unstable-v1" false)
protocolnew("${HYPRLAND_PROTOCOLS}/protocols" "hyprland-toplevel-export-v1"
            true)
protocolnew("protocols" "wlr-screencopy-unstable-v1" true)
protocolnew("protocols" "wlr-gamma-control-unstable-v1" true)
protocolnew("protocols" "wlr-foreign-toplevel-management-unstable-v1" true)
protocolnew("protocols" "wlr-output-power-management-unstable-v1" true)
protocolnew("protocols" "virtual-keyboard-unstable-v1" true)
protocolnew("protocols" "wlr-virtual-pointer-unstable-v1" true)
protocolnew("protocols" "input-method-unstable-v2" true)
protocolnew("protocols" "wlr-output-management-unstable-v1" true)
protocolnew("protocols" "kde-server-decoration" true)
protocolnew("protocols" "wlr-data-control-unstable-v1" true)
protocolnew("${HYPRLAND_PROTOCOLS}/protocols" "hyprland-focus-grab-v1" true)
protocolnew("protocols" "wlr-layer-shell-unstable-v1" true)
protocolnew("protocols" "xx-color-management-v4" true)
protocolnew("protocols" "frog-color-management-v1" true)
protocolnew("protocols" "wayland-drm" true)
protocolnew("${HYPRLAND_PROTOCOLS}/protocols" "hyprland-ctm-control-v1" true)
protocolnew("${HYPRLAND_PROTOCOLS}/protocols" "hyprland-surface-v1" true)
protocolnew("${HYPRLAND_PROTOCOLS}/protocols" "hyprland-lock-notify-v1" true)
protocolnew("${HYPRLAND_PROTOCOLS}/protocols" "hyprland-toplevel-mapping-v1"
            true)

protocolnew("staging/tearing-control" "tearing-control-v1" false)
protocolnew("staging/fractional-scale" "fractional-scale-v1" false)
protocolnew("unstable/xdg-output" "xdg-output-unstable-v1" false)
protocolnew("staging/cursor-shape" "cursor-shape-v1" false)
protocolnew("unstable/idle-inhibit" "idle-inhibit-unstable-v1" false)
protocolnew("unstable/relative-pointer" "relative-pointer-unstable-v1" false)
protocolnew("unstable/xdg-decoration" "xdg-decoration-unstable-v1" false)
protocolnew("staging/alpha-modifier" "alpha-modifier-v1" false)
protocolnew("staging/ext-foreign-toplevel-list" "ext-foreign-toplevel-list-v1"
            false)
protocolnew("unstable/pointer-gestures" "pointer-gestures-unstable-v1" false)
protocolnew("unstable/keyboard-shortcuts-inhibit"
            "keyboard-shortcuts-inhibit-unstable-v1" false)
protocolnew("unstable/text-input" "text-input-unstable-v3" false)
protocolnew("unstable/pointer-constraints" "pointer-constraints-unstable-v1"
            false)
protocolnew("staging/xdg-activation" "xdg-activation-v1" false)
protocolnew("staging/ext-idle-notify" "ext-idle-notify-v1" false)
protocolnew("staging/ext-session-lock" "ext-session-lock-v1" false)
protocolnew("stable/tablet" "tablet-v2" false)
protocolnew("stable/presentation-time" "presentation-time" false)
protocolnew("stable/xdg-shell" "xdg-shell" false)
protocolnew("unstable/primary-selection" "primary-selection-unstable-v1" false)
protocolnew("staging/xwayland-shell" "xwayland-shell-v1" false)
protocolnew("stable/viewporter" "viewporter" false)
protocolnew("stable/linux-dmabuf" "linux-dmabuf-v1" false)
protocolnew("staging/drm-lease" "drm-lease-v1" false)
protocolnew("staging/linux-drm-syncobj" "linux-drm-syncobj-v1" false)
protocolnew("staging/xdg-dialog" "xdg-dialog-v1" false)
protocolnew("staging/single-pixel-buffer" "single-pixel-buffer-v1" false)
protocolnew("staging/security-context" "security-context-v1" false)
protocolnew("staging/content-type" "content-type-v1" false)
protocolnew("staging/color-management" "color-management-v1" false)
protocolnew("staging/xdg-toplevel-tag" "xdg-toplevel-tag-v1" false)
protocolnew("staging/xdg-system-bell" "xdg-system-bell-v1" false)
protocolnew("staging/ext-workspace" "ext-workspace-v1" false)
protocolnew("staging/ext-data-control" "ext-data-control-v1" false)
protocolnew("staging/pointer-warp" "pointer-warp-v1" false)

protocolwayland()

# tools
add_subdirectory(hyprctl)

if(NO_HYPRPM)
  message(STATUS "hyprpm is disabled")
else()
  add_subdirectory(hyprpm)
  message(STATUS "hyprpm is enabled (NO_HYPRPM not defined)")
endif()

# binary and symlink
install(TARGETS Hyprland)

install(
  CODE "execute_process( \
        COMMAND ${CMAKE_COMMAND} -E create_symlink \
        ${CMAKE_INSTALL_FULL_BINDIR}/Hyprland \
        \"\$ENV{DESTDIR}${CMAKE_INSTALL_FULL_BINDIR}/hyprland\" \
        )")
# session file
install(FILES ${CMAKE_SOURCE_DIR}/example/hyprland.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/wayland-sessions)

# allow Hyprland to find assets
add_compile_definitions(DATAROOTDIR="${CMAKE_INSTALL_FULL_DATAROOTDIR}")

# installable assets
file(GLOB_RECURSE INSTALLABLE_ASSETS "assets/install/*")
list(FILTER INSTALLABLE_ASSETS EXCLUDE REGEX "meson.build")
install(FILES ${INSTALLABLE_ASSETS}
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/hypr)

# default config
install(FILES ${CMAKE_SOURCE_DIR}/example/hyprland.conf
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/hypr)

# portal config
install(FILES ${CMAKE_SOURCE_DIR}/assets/hyprland-portals.conf
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/xdg-desktop-portal)

# man pages
file(GLOB_RECURSE MANPAGES "docs/*.1")
install(FILES ${MANPAGES} DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

# pkgconfig entry
install(FILES ${CMAKE_BINARY_DIR}/hyprland.pc
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)

# protocol headers
set(HEADERS_PROTO "${CMAKE_CURRENT_SOURCE_DIR}/protocols")
install(
  DIRECTORY ${HEADERS_PROTO}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/hyprland
  FILES_MATCHING
  PATTERN "*.h*")

# hyprland headers
set(HEADERS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")
install(
  DIRECTORY ${HEADERS_SRC}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/hyprland
  FILES_MATCHING
  PATTERN "*.h"
  PATTERN "*.hpp"
  PATTERN "*.inc")

if(TESTS)
  message(STATUS "building tests is enabled TESTS")

  enable_testing()
  add_custom_target(tests)

  add_subdirectory(hyprtester)
  add_test(
    NAME "Main Test"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/hyprtester
    COMMAND hyprtester)

  add_dependencies(tests hyprtester)
else()
  message(STATUS "building tests is disabled")
endif()
