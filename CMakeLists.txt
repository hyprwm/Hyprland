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
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(
    COMMAND sh -c "git show ${GIT_COMMIT_HASH} | head -n 5 | tail -n 1 | sed -e 's/#//g' -e 's/\"//g'"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_MESSAGE
    OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(
    COMMAND sh -c "git diff-index --quiet HEAD -- || echo \"dirty\""
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_DIRTY
    OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(
    COMMAND sh -c "git describe --tags"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE)
#
#

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

if(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)
    message(STATUS "Setting debug flags")

    target_link_libraries(Hyprland asan)

    add_compile_options(-pg -no-pie -fno-builtin -fsanitize=address)
    add_link_options(-pg -no-pie -fno-builtin)
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
    pkg_check_modules(LIBSYSTEMD libsystemd)
    check_include_file("systemd/sd-daemon.h" SYSTEMDH)
    if(LIBSYSTEMD_FOUND AND SYSTEMDH)
        add_compile_definitions(USES_SYSTEMD)
        target_link_libraries(Hyprland "${LIBSYSTEMD_LIBRARIES}")
    else()
        message(WARNING "Systemd support requested but libsystemd or systemd headers were not found")
    endif()
endif()

target_compile_definitions(Hyprland
    PRIVATE
    "GIT_COMMIT_HASH=\"${GIT_COMMIT_HASH}\""
    "GIT_BRANCH=\"${GIT_BRANCH}\""
    "GIT_COMMIT_MESSAGE=\"${GIT_COMMIT_MESSAGE}\""
    "GIT_DIRTY=\"${GIT_DIRTY}\""
    "GIT_TAG=\"${GIT_TAG}\"")

target_link_libraries(Hyprland rt)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

message(STATUS "Setting link libraries")

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

target_link_libraries(Hyprland PkgConfig::deps)

target_link_libraries(Hyprland
        ${CMAKE_SOURCE_DIR}/subprojects/wlroots/build/libwlroots.so.12032 # wlroots is provided by us
        OpenGL::EGL
        OpenGL::GL
        Threads::Threads
        ${CMAKE_SOURCE_DIR}/subprojects/udis86/build/libudis86/liblibudis86.a
)

protocol("protocols/ext-workspace-unstable-v1.xml" "ext-workspace-unstable-v1" true)
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
protocol("staging/fractional-scale/fractional-scale-v1.xml" "fractional-scale-v1" false)
protocol("unstable/text-input/text-input-unstable-v1.xml" "text-input-unstable-v1" false)
