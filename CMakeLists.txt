cmake_minimum_required(VERSION 3.19)
include(CheckIncludeFile)

# Get version
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/props.json PROPS)
string(JSON VER GET ${PROPS} version)

project(Hyprland
    DESCRIPTION "A Modern C++ Wayland Compositor"
    VERSION ${VER}
)

set(HYPRLAND_VERSION ${VER})
set(PREFIX ${CMAKE_INSTALL_PREFIX})
configure_file(hyprland.pc.in hyprland.pc @ONLY) 

set(CMAKE_MESSAGE_LOG_LEVEL "STATUS")

message(STATUS "Gathering git info")

# Get git info
# hash and branch
execute_process(
    COMMAND ./scripts/generateVersion.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
#
#

# udis
add_subdirectory("subprojects/udis86")

# wlroots
message(STATUS "Setting up wlroots")

include(ExternalProject)

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
        set(BUILDTYPE_LOWER "empty")
    else()
        set(BUILDTYPE_LOWER "release")
    endif()
else()
    set(BUILDTYPE_LOWER "release")
endif()

ExternalProject_Add(
    wlroots
    PREFIX ${CMAKE_SOURCE_DIR}/subprojects/wlroots
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/subprojects/wlroots
    PATCH_COMMAND sed -E -i -e "s/(soversion = 12)([^032]|$$)/soversion = 12032/g" meson.build
    CONFIGURE_COMMAND meson setup build --buildtype=${BUILDTYPE_LOWER} -Dwerror=false -Dexamples=false -Drenderers=gles2 $<IF:$<BOOL:${WITH_ASAN}>,-Db_sanitize=address,-Db_sanitize=none> && meson setup build --buildtype=${BUILDTYPE_LOWER} -Dwerror=false -Dexamples=false -Drenderers=gles2 $<IF:$<BOOL:${WITH_ASAN}>,-Db_sanitize=address,-Db_sanitize=none> --reconfigure
    BUILD_COMMAND ninja -C build
    BUILD_ALWAYS true
    BUILD_IN_SOURCE true
    BUILD_BYPRODUCTS ${CMAKE_SOURCE_DIR}/subprojects/wlroots/build/libwlroots.so.12032
    INSTALL_COMMAND echo "wlroots: install not needed"
)

find_program(WaylandScanner NAMES wayland-scanner)
message(STATUS "Found WaylandScanner at ${WaylandScanner}")
execute_process(
    COMMAND pkg-config --variable=pkgdatadir wayland-protocols
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE WAYLAND_PROTOCOLS_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE)
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
  "subprojects/wlroots/include/"
  "subprojects/wlroots/build/include/"
  "subprojects/udis86/"
  "protocols/")
set(CMAKE_CXX_STANDARD 23)
add_compile_definitions(WLR_USE_UNSTABLE)
add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-value -Wno-missing-field-initializers -Wno-narrowing -Wno-pointer-arith)
add_link_options(-rdynamic)
set(CMAKE_ENABLE_EXPORTS TRUE)

message(STATUS "Checking deps...")

find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)
find_package(OpenGL REQUIRED)
pkg_check_modules(deps REQUIRED IMPORTED_TARGET wayland-server wayland-client wayland-cursor wayland-protocols cairo libdrm xkbcommon libinput pango pangocairo pixman-1) # we do not check for wlroots, as we provide it ourselves

file(GLOB_RECURSE SRCFILES CONFIGURE_DEPENDS "src/*.cpp")

add_executable(Hyprland ${SRCFILES})
add_dependencies(Hyprland wlroots)

if(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)
    message(STATUS "Setting debug flags")

    if (WITH_ASAN)
        message(STATUS "Enabling ASan")

        target_link_libraries(Hyprland asan)
        add_compile_options(-fsanitize=address)
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

if(LEGACY_RENDERER)
    message(STATUS "Using the legacy GLES2 renderer!")
    add_compile_definitions(LEGACY_RENDERER)
endif()

if(NO_XWAYLAND)
    message(STATUS "Using the NO_XWAYLAND flag, disabling XWayland!")
    add_compile_definitions(NO_XWAYLAND)
else()
    message(STATUS "XWAYLAND Enabled (NO_XWAYLAND not defined) checking deps...")
    pkg_check_modules(xcbdep REQUIRED IMPORTED_TARGET xcb)
    target_link_libraries(Hyprland PkgConfig::xcbdep)
endif()

if(NO_SYSTEMD)
    message(STATUS "SYSTEMD support is disabled...")
else()
    message(STATUS "SYSTEMD support is requested (NO_SYSTEMD not defined) checking deps...")
    check_include_file("systemd/sd-daemon.h" SYSTEMDH)
    if(SYSTEMDH)
        pkg_check_modules(LIBSYSTEMD libsystemd)
        if (LIBSYSTEMD_FOUND)
            add_compile_definitions(USES_SYSTEMD)
            target_link_libraries(Hyprland "${LIBSYSTEMD_LIBRARIES}")
            message(STATUS "Systemd found")
        else()
            message(WARNING "Systemd support requested but systemd libraries were not found")
        endif()
    else()
        message(WARNING "Systemd support requested but systemd headers were not found")
    endif()
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

target_link_libraries(Hyprland
        ${CMAKE_SOURCE_DIR}/subprojects/wlroots/build/libwlroots.so.12032 # wlroots is provided by us
        OpenGL::EGL
        OpenGL::GL
        Threads::Threads
        libudis86
)

protocol("protocols/idle.xml" "idle" true)
protocol("protocols/pointer-constraints-unstable-v1.xml" "pointer-constraints-unstable-v1" true)
protocol("protocols/tablet-unstable-v2.xml" "tablet-unstable-v2" true)
protocol("protocols/wlr-foreign-toplevel-management-unstable-v1.xml" "wlr-foreign-toplevel-management-unstable-v1" true)
protocol("protocols/wlr-layer-shell-unstable-v1.xml" "wlr-layer-shell-unstable-v1" true)
protocol("protocols/wlr-output-power-management-unstable-v1.xml" "wlr-output-power-management-unstable-v1" true)
protocol("protocols/wlr-screencopy-unstable-v1.xml" "wlr-screencopy-unstable-v1" true)
protocol("subprojects/hyprland-protocols/protocols/hyprland-global-shortcuts-v1.xml" "hyprland-global-shortcuts-v1" true)
protocol("subprojects/hyprland-protocols/protocols/hyprland-toplevel-export-v1.xml" "hyprland-toplevel-export-v1" true)
protocol("stable/xdg-shell/xdg-shell.xml" "xdg-shell" false)
protocol("unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml" "linux-dmabuf-unstable-v1" false)
protocol("unstable/xdg-output/xdg-output-unstable-v1.xml" "xdg-output-unstable-v1" false)
protocol("staging/fractional-scale/fractional-scale-v1.xml" "fractional-scale-v1" false)
protocol("staging/tearing-control/tearing-control-v1.xml" "tearing-control-v1" false)
protocol("unstable/text-input/text-input-unstable-v1.xml" "text-input-unstable-v1" false)
protocol("staging/cursor-shape/cursor-shape-v1.xml" "cursor-shape-v1" false)

# hyprctl
add_subdirectory(hyprctl)
