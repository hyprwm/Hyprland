cmake_minimum_required(VERSION 3.19)

project(
    hyprctl
    DESCRIPTION "Control utility for Hyprland"
)

pkg_check_modules(hyprctl_deps REQUIRED IMPORTED_TARGET hyprutils>=0.2.4 hyprwire re2)

file(GLOB_RECURSE HYPRCTL_SRCFILES CONFIGURE_DEPENDS "src/*.cpp" "hw-protocols/*.cpp" "include/*.hpp")

add_executable(hyprctl ${HYPRCTL_SRCFILES})

target_link_libraries(hyprctl PUBLIC PkgConfig::hyprctl_deps)
target_include_directories(hyprctl PRIVATE "hw-protocols")

# Hyprwire

pkg_get_variable(HYPRWIRE_PROTOCOLS_DIR hyprwire-protocols pkgdatadir)
message(STATUS "Found hyprwire-protocols at ${HYPRWIRE_PROTOCOLS_DIR}")

function(hyprprotocol protoPath protoName external)
  if(external)
    set(path ${protoPath})
  else()
    set(path ${HYPRWIRE_PROTOCOLS_DIR}/${protoPath})
  endif()
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/hw-protocols/${protoName}-client.cpp
           ${CMAKE_CURRENT_SOURCE_DIR}/hw-protocols/${protoName}-client.hpp
    COMMAND hyprwire-scanner --client ${path}/${protoName}.xml
            ${CMAKE_CURRENT_SOURCE_DIR}/hw-protocols/
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  target_sources(hyprctl PRIVATE hw-protocols/${protoName}-client.cpp
                                   hw-protocols/${protoName}-client.hpp
                                   )
endfunction()

hyprprotocol(hw-protocols hyprpaper_core true)
hyprprotocol(hyprtavern hp_hyprtavern_core_v1 false)

# binary
install(TARGETS hyprctl)

# shell completions
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/hyprctl.bash
        DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
        RENAME hyprctl)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/hyprctl.fish
        DESTINATION ${CMAKE_INSTALL_DATADIR}/fish/vendor_completions.d)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/hyprctl.zsh
        DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions
        RENAME _hyprctl)
