cmake_minimum_required(VERSION 3.19)
include(CheckIncludeFile)

# Get version
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/props.json PROPS)
string(JSON VER GET ${PROPS} version)

project(Hyprland
    DESCRIPTION "A Modern C++ Wayland Compositor"
    VERSION ${VER}
)

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
#
#

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
  "subprojects/udis86/")
set(CMAKE_CXX_STANDARD 23)
add_compile_definitions(WLR_USE_UNSTABLE)
add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-value -Wno-missing-field-initializers -Wno-narrowing -Wno-pointer-arith)
add_link_options(-rdynamic)
set(CMAKE_ENABLE_EXPORTS TRUE)

message(STATUS "Checking deps...")

find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(deps REQUIRED IMPORTED_TARGET wayland-server wayland-client wayland-cursor wayland-protocols cairo libdrm egl xkbcommon libinput pango pangocairo) # we do not check for wlroots, as we provide it ourselves

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
    target_link_libraries(Hyprland PRIVATE execinfo)
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
    pkg_check_modules(xcbdep REQUIRED xcb)
    target_link_libraries(Hyprland xcb)
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
    "GIT_DIRTY=\"${GIT_DIRTY}\"")

target_link_libraries(Hyprland rt)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

message(STATUS "Setting link libraries")

target_link_libraries(Hyprland PkgConfig::deps)

target_link_libraries(Hyprland
        ${CMAKE_SOURCE_DIR}/subprojects/wlroots/build/libwlroots.so.12032 # wlroots is provided by us
        pixman-1
        OpenGL
        GLESv2
        pthread
        ${CMAKE_THREAD_LIBS_INIT}
        ${CMAKE_SOURCE_DIR}/ext-workspace-unstable-v1-protocol.o
        ${CMAKE_SOURCE_DIR}/wlr-foreign-toplevel-management-unstable-v1-protocol.o
        ${CMAKE_SOURCE_DIR}/hyprland-toplevel-export-v1-protocol.o
        ${CMAKE_SOURCE_DIR}/hyprland-global-shortcuts-v1-protocol.o
        ${CMAKE_SOURCE_DIR}/fractional-scale-v1-protocol.o
        ${CMAKE_SOURCE_DIR}/text-input-unstable-v1-protocol.o
        ${CMAKE_SOURCE_DIR}/wlr-screencopy-unstable-v1-protocol.o
        ${CMAKE_SOURCE_DIR}/subprojects/udis86/build/libudis86/liblibudis86.a
)
