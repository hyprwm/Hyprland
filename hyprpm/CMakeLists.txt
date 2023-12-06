cmake_minimum_required(VERSION 3.19)

project(
    hyprpm
    DESCRIPTION "A Hyprland Plugin Manager"
)

file(GLOB_RECURSE SRCFILES CONFIGURE_DEPENDS "src/*.cpp")

set(CMAKE_CXX_STANDARD 23)

add_executable(hyprpm ${SRCFILES})
target_include_directories(hyprpm PUBLIC "." "subprojects/tomlpp/include")
