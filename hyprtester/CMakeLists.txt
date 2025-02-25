cmake_minimum_required(VERSION 3.19)

project(hyprtester DESCRIPTION "Hyprland test suite")

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 26)

find_package(PkgConfig REQUIRED)

pkg_check_modules(hyprtester_deps REQUIRED IMPORTED_TARGET hyprutils>=0.5.0)

file(GLOB_RECURSE SRCFILES CONFIGURE_DEPENDS "src/*.cpp")

add_executable(hyprtester ${SRCFILES})
add_custom_command(TARGET hyprtester POST_BUILD COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/plugin/build.sh WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/plugin)

target_link_libraries(hyprtester PUBLIC PkgConfig::hyprtester_deps)

install(TARGETS hyprtester)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/test.conf
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/hypr)
