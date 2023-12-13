cmake_minimum_required(VERSION 3.19)

project(
    hyprpm
    DESCRIPTION "A Hyprland Plugin Manager"
)

file(GLOB_RECURSE SRCFILES CONFIGURE_DEPENDS "src/*.cpp")

set(CMAKE_CXX_STANDARD 23)

pkg_check_modules(tomlplusplus REQUIRED IMPORTED_TARGET tomlplusplus)

add_executable(hyprpm ${SRCFILES})

target_link_libraries(hyprpm PUBLIC PkgConfig::tomlplusplus)
