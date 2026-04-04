cmake_minimum_required(VERSION 3.19)

project(start-hyprland DESCRIPTION "Hyprland watchdog binary")

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

find_package(PkgConfig REQUIRED)

pkg_check_modules(starthyprland_deps REQUIRED IMPORTED_TARGET hyprutils>=0.10.3)

find_package(glaze 7...<8 QUIET)
if (NOT glaze_FOUND)
    set(GLAZE_VERSION v7.2.0)
    message(STATUS "start: glaze dependency not found, retrieving ${GLAZE_VERSION} with FetchContent")
    include(FetchContent)
    FetchContent_Declare(
        glaze
        GIT_REPOSITORY https://github.com/stephenberry/glaze.git
        GIT_TAG ${GLAZE_VERSION}
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(glaze)
endif()

file(GLOB_RECURSE SRCFILES CONFIGURE_DEPENDS "src/*.cpp")

add_executable(start-hyprland ${SRCFILES})

target_link_libraries(start-hyprland PUBLIC PkgConfig::starthyprland_deps)
target_link_libraries(start-hyprland PUBLIC glaze::glaze)

install(TARGETS start-hyprland)

if(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)
  set(CMAKE_EXECUTABLE_ENABLE_EXPORTS TRUE)
endif()
