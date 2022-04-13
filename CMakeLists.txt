cmake_minimum_required(VERSION 3.4)
project(Hyprland 
    DESCRIPTION "A Modern C++ Wayland Compositor"
)

set(CMAKE_MESSAGE_LOG_LEVEL "STATUS")

message(STATUS "Configuring Hyprland!")

include_directories(.)
add_compile_options(-std=c++20 -DWLR_USE_UNSTABLE )
add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-value -Wno-missing-field-initializers -Wno-narrowing)
find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(deps REQUIRED IMPORTED_TARGET wayland-server wayland-client wayland-cursor wayland-protocols cairo pango pangocairo libdrm egl xkbcommon wlroots libinput xcb)

file(GLOB_RECURSE SRCFILES "src/*.cpp")

add_executable(Hyprland ${SRCFILES})

IF(LEGACY_RENDERER MATCHES true)
    message(STATUS "Using the legacy GLES2 renderer!")
    add_definitions( -DLEGACY_RENDERER )
ENDIF(LEGACY_RENDERER MATCHES true)

IF(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)
    message(STATUS "Configuring Hyprland in Debug with CMake!")
ELSE()
    add_compile_options(-Ofast)
    message(STATUS "Configuring Hyprland in Release with CMake!")
ENDIF(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)

target_link_libraries(Hyprland rt)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

target_link_libraries(Hyprland PkgConfig::deps)

target_link_libraries(Hyprland
        wlroots
        pixman-1
        OpenGL
        GLESv2
        pthread
        ${CMAKE_THREAD_LIBS_INIT}
        ${CMAKE_SOURCE_DIR}/ext-workspace-unstable-v1-protocol.o
)

IF(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg -no-pie -fno-builtin")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg -no-pie -fno-builtin")
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pg -no-pie -fno-builtin")
ENDIF(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)