cmake_minimum_required(VERSION 3.19)

project(hyprtester DESCRIPTION "Hyprland test suite")

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

find_package(PkgConfig REQUIRED)

pkg_check_modules(hyprtester_deps REQUIRED IMPORTED_TARGET hyprutils>=0.5.0)

file(GLOB_RECURSE SRCFILES CONFIGURE_DEPENDS "src/*.cpp")

add_executable(hyprtester ${SRCFILES})
add_custom_command(
  TARGET hyprtester
  POST_BUILD
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/plugin/build.sh
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/plugin)

target_link_libraries(hyprtester PUBLIC PkgConfig::hyprtester_deps)

install(TARGETS hyprtester)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/test.conf
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/hypr)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/plugin/hyprtestplugin.so
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

######## wayland protocols testing stuff

if(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)
  set(CMAKE_EXECUTABLE_ENABLE_EXPORTS TRUE)
endif()

find_package(hyprwayland-scanner 0.4.0 REQUIRED)
pkg_check_modules(
  protocols_deps
  REQUIRED
  IMPORTED_TARGET
  hyprutils>=0.8.0
  wayland-client
  wayland-protocols
)

pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
message(STATUS "Found wayland-protocols at ${WAYLAND_PROTOCOLS_DIR}")
pkg_get_variable(WAYLAND_SCANNER_PKGDATA_DIR wayland-scanner pkgdatadir)
message(STATUS "Found wayland-scanner pkgdatadir at ${WAYLAND_SCANNER_PKGDATA_DIR}")

# gen core wayland stuff
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wayland.cpp
         ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wayland.hpp
  COMMAND hyprwayland-scanner --wayland-enums --client
          ${WAYLAND_SCANNER_PKGDATA_DIR}/wayland.xml ${CMAKE_CURRENT_SOURCE_DIR}/protocols/
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

function(protocolNew protoPath protoName external)
  if(external)
    set(path ${CMAKE_CURRENT_SOURCE_DIR}/${protoPath})
  else()
    set(path ${WAYLAND_PROTOCOLS_DIR}/${protoPath})
  endif()

  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/protocols/${protoName}.cpp
           ${CMAKE_CURRENT_SOURCE_DIR}/protocols/${protoName}.hpp
    COMMAND hyprwayland-scanner --client ${path}/${protoName}.xml
            ${CMAKE_CURRENT_SOURCE_DIR}/protocols/
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endfunction()
function(clientNew sourceName)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "PROTOS")

  add_executable(${sourceName} clients/${sourceName}.cpp)

  target_include_directories(${sourceName} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/protocols")
  target_link_libraries(${sourceName} PUBLIC PkgConfig::protocols_deps)

  target_sources(${sourceName} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wayland.cpp ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wayland.hpp)

  foreach(protoName IN LISTS ARG_PROTOS)
    target_sources(${sourceName} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/protocols/${protoName}.cpp
                                         ${CMAKE_CURRENT_SOURCE_DIR}/protocols/${protoName}.hpp)
  endforeach()
endfunction()

protocolnew("staging/pointer-warp" "pointer-warp-v1" false)
protocolnew("stable/xdg-shell" "xdg-shell" false)

clientNew("pointer-warp" PROTOS "pointer-warp-v1" "xdg-shell")
