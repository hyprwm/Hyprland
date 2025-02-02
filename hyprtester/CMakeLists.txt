cmake_minimum_required(VERSION 3.19)

project(
    hyprtester
    DESCRIPTION "Hyprland test suite"
)

set(CMAKE_CXX_STANDARD 26)

pkg_check_modules(hyprtester_deps REQUIRED IMPORTED_TARGET hyprutils>=0.5.0)

file(GLOB_RECURSE SRCFILES CONFIGURE_DEPENDS "src/*.cpp")

add_executable(hyprtester ${SRCFILES})

target_link_libraries(hyprtester PUBLIC PkgConfig::hyprtester_deps)

