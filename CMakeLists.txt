cmake_minimum_required(VERSION 3.4)
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

IF(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)
    message(STATUS "Configuring Hyprland in Debug with CMake")
    add_definitions( -DHYPRLAND_DEBUG )
ELSE()
    add_compile_options( -O3 )
    message(STATUS "Configuring Hyprland in Release with CMake")
ENDIF(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)

include_directories(. PRIVATE "subprojects/wlroots/include/")
include_directories(. PRIVATE "subprojects/wlroots/build/include/")
set(CMAKE_CXX_STANDARD 23)
add_compile_options(-DWLR_USE_UNSTABLE)
add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-value -Wno-missing-field-initializers -Wno-narrowing)

message(STATUS "Checking deps...")

find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(deps REQUIRED IMPORTED_TARGET wayland-server wayland-client wayland-cursor wayland-protocols cairo libdrm egl xkbcommon libinput) # we do not check for wlroots, as we provide it ourselves

file(GLOB_RECURSE SRCFILES "src/*.cpp")

add_executable(Hyprland ${SRCFILES})

IF(LEGACY_RENDERER MATCHES true)
    message(STATUS "Using the legacy GLES2 renderer!")
    add_definitions( -DLEGACY_RENDERER )
ENDIF(LEGACY_RENDERER MATCHES true)

IF(NO_XWAYLAND MATCHES true)
    message(STATUS "Using the NO_XWAYLAND flag, disabling XWayland!")
    add_definitions( -DNO_XWAYLAND )
ELSE()
    message(STATUS "XWAYLAND Enabled (NO_XWAYLAND not defined) checking deps...")
    pkg_check_modules(xcbdep REQUIRED xcb)
    target_link_libraries(Hyprland xcb)
ENDIF(NO_XWAYLAND MATCHES true)

IF(NO_SYSTEMD MATCHES true)
    message(STATUS "SYSTEMD support is disabled...")
ELSE()
    message(STATUS "SYSTEMD support is requested (NO_SYSTEMD not defined) checking deps...")
    pkg_check_modules(LIBSYSTEMD libsystemd)
    check_include_file("systemd/sd-daemon.h" SYSTEMDH)
    IF(LIBSYSTEMD_FOUND AND SYSTEMDH)
        add_definitions( -DUSES_SYSTEMD )
        target_link_libraries(Hyprland "${LIBSYSTEMD_LIBRARIES}")
    ELSE()
        message(WARNING "Systemd support requested but libsystemd or systemd headers were not found")
    ENDIF(LIBSYSTEMD_FOUND AND SYSTEMDH)
ENDIF(NO_SYSTEMD MATCHES true)

target_compile_definitions(Hyprland PRIVATE "-DGIT_COMMIT_HASH=\"${GIT_COMMIT_HASH}\"")
target_compile_definitions(Hyprland PRIVATE "-DGIT_BRANCH=\"${GIT_BRANCH}\"")
target_compile_definitions(Hyprland PRIVATE "-DGIT_COMMIT_MESSAGE=\"${GIT_COMMIT_MESSAGE}\"")
target_compile_definitions(Hyprland PRIVATE "-DGIT_DIRTY=\"${GIT_DIRTY}\"")

target_link_libraries(Hyprland rt)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

message(STATUS "Setting link libraries")

target_link_libraries(Hyprland PkgConfig::deps)

IF(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)
    message(STATUS "Setting debug flags")

    target_link_libraries(Hyprland asan)

    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg -no-pie -fno-builtin -fsanitize=address")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg -no-pie -fno-builtin")
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pg -no-pie -fno-builtin")
ENDIF(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)

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
)
