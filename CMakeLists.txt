cmake_minimum_required(VERSION 3.27)

include(CheckIncludeFile)
include(ExternalProject)
include(GNUInstallDirs)

# Get version
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/props.json PROPS)
string(JSON VER GET ${PROPS} version)

project(Hyprland
    DESCRIPTION "A Modern C++ Wayland Compositor"
    VERSION ${VER}
)

set(HYPRLAND_VERSION ${VER})
set(PREFIX ${CMAKE_INSTALL_PREFIX})
set(INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR})
configure_file(hyprland.pc.in hyprland.pc @ONLY) 

set(CMAKE_MESSAGE_LOG_LEVEL "STATUS")

message(STATUS "Gathering git info")

# Get git info
# hash and branch
execute_process(
    COMMAND ./scripts/generateVersion.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# udis
add_subdirectory("subprojects/udis86")

# wlroots
message(STATUS "Setting up wlroots")

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

ExternalProject_Add(
    wlroots-hyprland
    PREFIX ${CMAKE_SOURCE_DIR}/subprojects/wlroots-hyprland
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/subprojects/wlroots-hyprland
    CONFIGURE_COMMAND meson setup --reconfigure build --buildtype=${BUILDTYPE_LOWER} -Dwerror=false -Dxwayland=$<IF:$<BOOL:${NO_XWAYLAND}>,disabled,enabled> -Dexamples=false -Drenderers=gles2 -Dbackends=drm,libinput $<IF:$<BOOL:${WITH_ASAN}>,-Db_sanitize=address,-Db_sanitize=none>
    BUILD_COMMAND ninja -C build
    BUILD_ALWAYS true
    BUILD_IN_SOURCE true
    BUILD_BYPRODUCTS ${CMAKE_SOURCE_DIR}/subprojects/wlroots-hyprland/build/libwlroots.a
    INSTALL_COMMAND echo "wlroots-hyprland: install not needed"
)

find_package(PkgConfig REQUIRED)

pkg_get_variable(WaylandScanner wayland-scanner wayland_scanner)
message(STATUS "Found WaylandScanner at ${WaylandScanner}")
pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
message(STATUS "Found wayland-protocols at ${WAYLAND_PROTOCOLS_DIR}")

if(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)
    message(STATUS "Configuring Hyprland in Debug with CMake")
    add_compile_definitions(HYPRLAND_DEBUG)
else()
    add_compile_options(-O3)
    message(STATUS "Configuring Hyprland in Release with CMake")
endif()

include_directories(
  .
  "src/"
  "subprojects/wlroots-hyprland/include/"
  "subprojects/wlroots-hyprland/build/include/"
  "subprojects/udis86/"
  "protocols/")
set(CMAKE_CXX_STANDARD 23)
add_compile_definitions(WLR_USE_UNSTABLE)
add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-value -Wno-missing-field-initializers -Wno-narrowing -Wno-pointer-arith)

set(CMAKE_EXECUTABLE_ENABLE_EXPORTS TRUE)

message(STATUS "Checking deps...")

find_package(Threads REQUIRED)

if(LEGACY_RENDERER)
    set(GLES_VERSION "GLES2")
else()
    set(GLES_VERSION "GLES3")
endif()
find_package(OpenGL REQUIRED COMPONENTS ${GLES_VERSION})

pkg_check_modules(deps REQUIRED IMPORTED_TARGET
    xkbcommon uuid
    wayland-server wayland-client wayland-cursor wayland-protocols
    cairo pango pangocairo pixman-1
    libdrm libinput hwdata libseat libdisplay-info libliftoff libudev gbm
    hyprwayland-scanner>=0.3.6 hyprlang>=0.3.2 hyprcursor>=0.1.7
)

file(GLOB_RECURSE SRCFILES "src/*.cpp")

set(TRACY_CPP_FILES "")
if(USE_TRACY)
    set(TRACY_CPP_FILES "subprojects/tracy/public/TracyClient.cpp")
    message(STATUS "Tracy enabled, TRACY_CPP_FILES: " ${TRACY_CPP_FILES})
endif()

add_executable(Hyprland ${SRCFILES} ${TRACY_CPP_FILES})
add_dependencies(Hyprland wlroots-hyprland)

if(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)
    message(STATUS "Setting debug flags")

    if (WITH_ASAN)
        message(STATUS "Enabling ASan")

        target_link_libraries(Hyprland asan)
        target_compile_options(Hyprland PUBLIC -fsanitize=address)
    endif()

    if(USE_TRACY)
        message(STATUS "Tracy is turned on")

        option( TRACY_ENABLE "" ON)
        option( TRACY_ON_DEMAND "" ON)
        add_subdirectory (subprojects/tracy)

        target_link_libraries(Hyprland Tracy::TracyClient)

        if(USE_TRACY_GPU)
            message(STATUS "Tracy GPU Profiling is turned on")
            add_compile_definitions(USE_TRACY_GPU)
        endif()
    endif()

    add_compile_options(-pg -no-pie -fno-builtin)
    add_link_options(-pg -no-pie -fno-builtin)
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

if(LEGACY_RENDERER)
    message(STATUS "Using the legacy GLES2 renderer!")
    add_compile_definitions(LEGACY_RENDERER)
endif()

if(NO_XWAYLAND)
    message(STATUS "Using the NO_XWAYLAND flag, disabling XWayland!")
    add_compile_definitions(NO_XWAYLAND)
else()
    message(STATUS "XWAYLAND Enabled (NO_XWAYLAND not defined) checking deps...")
    pkg_check_modules(xdeps REQUIRED IMPORTED_TARGET xcb xwayland xcb-util xcb-render xcb-xfixes xcb-icccm xcb-composite xcb-res xcb-ewmh)
    pkg_check_modules(xcb_errors IMPORTED_TARGET xcb-errors)
    target_link_libraries(Hyprland PkgConfig::xdeps)
    if(xcb_errors_FOUND)
        target_link_libraries(Hyprland PkgConfig::xcb_errors)
    endif()
endif()

if(NO_SYSTEMD)
    message(STATUS "SYSTEMD support is disabled...")
else()
    message(STATUS "SYSTEMD support is requested (NO_SYSTEMD not defined)...")
    add_compile_definitions(USES_SYSTEMD)
endif()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

message(STATUS "Setting precompiled headers")

target_precompile_headers(Hyprland PRIVATE $<$<COMPILE_LANGUAGE:CXX>:src/pch/pch.hpp>)

message(STATUS "Setting link libraries")

target_link_libraries(Hyprland rt PkgConfig::deps)

function(protocol protoPath protoName external)
    if (external)
        execute_process(
            COMMAND ${WaylandScanner} server-header ${protoPath} protocols/${protoName}-protocol.h
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        execute_process(
            COMMAND ${WaylandScanner} private-code ${protoPath} protocols/${protoName}-protocol.c
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})  
        target_sources(Hyprland PRIVATE protocols/${protoName}-protocol.c)
    else()
        execute_process(
            COMMAND ${WaylandScanner} server-header ${WAYLAND_PROTOCOLS_DIR}/${protoPath} protocols/${protoName}-protocol.h
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        execute_process(
            COMMAND ${WaylandScanner} private-code ${WAYLAND_PROTOCOLS_DIR}/${protoPath} protocols/${protoName}-protocol.c
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})  
        target_sources(Hyprland PRIVATE protocols/${protoName}-protocol.c)
    endif()
endfunction()
function(protocolNew protoPath protoName external)
    if (external)
        execute_process(
            COMMAND hyprwayland-scanner ${protoPath} ${CMAKE_SOURCE_DIR}/protocols/
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        target_sources(Hyprland PRIVATE protocols/${protoName}.cpp)
    else()
        execute_process(
            COMMAND hyprwayland-scanner ${WAYLAND_PROTOCOLS_DIR}/${protoPath} ${CMAKE_SOURCE_DIR}/protocols/
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        target_sources(Hyprland PRIVATE protocols/${protoName}.cpp)
    endif()
endfunction()

target_link_libraries(Hyprland
        ${CMAKE_SOURCE_DIR}/subprojects/wlroots-hyprland/build/libwlroots.a
        OpenGL::EGL
        OpenGL::GL
        Threads::Threads
        libudis86
        uuid
)

protocol("protocols/wlr-screencopy-unstable-v1.xml" "wlr-screencopy-unstable-v1" true)
protocol("subprojects/hyprland-protocols/protocols/hyprland-global-shortcuts-v1.xml" "hyprland-global-shortcuts-v1" true)
protocol("subprojects/hyprland-protocols/protocols/hyprland-toplevel-export-v1.xml" "hyprland-toplevel-export-v1" true)
protocol("stable/xdg-shell/xdg-shell.xml" "xdg-shell" false)
protocol("unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml" "linux-dmabuf-unstable-v1" false)
protocol("unstable/text-input/text-input-unstable-v1.xml" "text-input-unstable-v1" false)

protocolNew("protocols/wlr-gamma-control-unstable-v1.xml" "wlr-gamma-control-unstable-v1" true)
protocolNew("protocols/wlr-foreign-toplevel-management-unstable-v1.xml" "wlr-foreign-toplevel-management-unstable-v1" true)
protocolNew("protocols/wlr-output-power-management-unstable-v1.xml" "wlr-output-power-management-unstable-v1" true)
protocolNew("protocols/virtual-keyboard-unstable-v1.xml" "virtual-keyboard-unstable-v1" true)
protocolNew("protocols/wlr-virtual-pointer-unstable-v1.xml" "wlr-virtual-pointer-unstable-v1" true)
protocolNew("protocols/input-method-unstable-v2.xml" "input-method-unstable-v2" true)
protocolNew("protocols/wlr-output-management-unstable-v1.xml" "wlr-output-management-unstable-v1" true)
protocolNew("protocols/kde-server-decoration.xml" "kde-server-decoration" true)
protocolNew("subprojects/hyprland-protocols/protocols/hyprland-focus-grab-v1.xml" "hyprland-focus-grab-v1" true)
protocolNew("protocols/wlr-layer-shell-unstable-v1.xml" "wlr-layer-shell-unstable-v1" true)
protocolNew("staging/tearing-control/tearing-control-v1.xml" "tearing-control-v1" false)
protocolNew("staging/fractional-scale/fractional-scale-v1.xml" "fractional-scale-v1" false)
protocolNew("unstable/xdg-output/xdg-output-unstable-v1.xml" "xdg-output-unstable-v1" false)
protocolNew("staging/cursor-shape/cursor-shape-v1.xml" "cursor-shape-v1" false)
protocolNew("unstable/idle-inhibit/idle-inhibit-unstable-v1.xml" "idle-inhibit-unstable-v1" false)
protocolNew("unstable/relative-pointer/relative-pointer-unstable-v1.xml" "relative-pointer-unstable-v1" false)
protocolNew("unstable/xdg-decoration/xdg-decoration-unstable-v1.xml" "xdg-decoration-unstable-v1" false)
protocolNew("staging/alpha-modifier/alpha-modifier-v1.xml" "alpha-modifier-v1" false)
protocolNew("staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml" "ext-foreign-toplevel-list-v1" false)
protocolNew("unstable/pointer-gestures/pointer-gestures-unstable-v1.xml" "pointer-gestures-unstable-v1" false)
protocolNew("unstable/keyboard-shortcuts-inhibit/keyboard-shortcuts-inhibit-unstable-v1.xml" "keyboard-shortcuts-inhibit-unstable-v1" false)
protocolNew("unstable/text-input/text-input-unstable-v3.xml" "text-input-unstable-v3" false)
protocolNew("unstable/pointer-constraints/pointer-constraints-unstable-v1.xml" "pointer-constraints-unstable-v1" false)
protocolNew("staging/xdg-activation/xdg-activation-v1.xml" "xdg-activation-v1" false)
protocolNew("staging/ext-idle-notify/ext-idle-notify-v1.xml" "ext-idle-notify-v1" false)
protocolNew("staging/ext-session-lock/ext-session-lock-v1.xml" "ext-session-lock-v1" false)
protocolNew("stable/tablet/tablet-v2.xml" "tablet-v2" false)
protocolNew("stable/presentation-time/presentation-time.xml" "presentation-time" false)

# tools
add_subdirectory(hyprctl)
add_subdirectory(hyprpm)

# binary and symlink
install(TARGETS Hyprland)

install(CODE "execute_process( \
        COMMAND ${CMAKE_COMMAND} -E create_symlink \
        ${CMAKE_INSTALL_BINDIR}/Hyprland \
        ${CMAKE_INSTALL_BINDIR}/hyprland
        )"
)

# session file
install(FILES ${CMAKE_SOURCE_DIR}/example/hyprland.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/wayland-sessions)

# wallpapers
file(GLOB_RECURSE WALLPAPERS "assets/wall*")
install(FILES ${WALLPAPERS}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/hyprland)

# default config
install(FILES ${CMAKE_SOURCE_DIR}/example/hyprland.conf
        DESTINATION ${CMAKE_INSTALL_DATADIR}/hyprland)

# portal config
install(FILES ${CMAKE_SOURCE_DIR}/assets/hyprland-portals.conf
        DESTINATION ${CMAKE_INSTALL_DATADIR}/xdg-desktop-portal)

# man pages
file(GLOB_RECURSE MANPAGES "docs/*.1")
install(FILES ${MANPAGES}
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)


# pkgconfig entry
install(FILES ${CMAKE_BINARY_DIR}/hyprland.pc
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)

# wlroots headers
set(HEADERS_WLR "${CMAKE_CURRENT_SOURCE_DIR}/subprojects/wlroots-hyprland/include/wlr")
install(DIRECTORY ${HEADERS_WLR}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/hyprland
        FILES_MATCHING PATTERN "*.h")

# config.h and version.h
set(HEADERS_WLR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/subprojects/wlroots-hyprland/build/include/wlr")
install(DIRECTORY ${HEADERS_WLR_ROOT}/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/hyprland/wlr
        FILES_MATCHING PATTERN "*.h")

# protocol headers
set(HEADERS_PROTO "${CMAKE_CURRENT_SOURCE_DIR}/protocols")
install(DIRECTORY ${HEADERS_PROTO}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/hyprland
        FILES_MATCHING PATTERN "*.h*")

# hyprland headers
set(HEADERS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")
install(DIRECTORY ${HEADERS_SRC}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/hyprland
        FILES_MATCHING PATTERN "*.h*")
