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

function(hyprprotocol protoPath protoName)
  set(path ${CMAKE_CURRENT_SOURCE_DIR}/${protoPath})
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/hw-protocols/${protoName}-client.cpp
           ${CMAKE_CURRENT_SOURCE_DIR}/hw-protocols/${protoName}-client.hpp
           ${CMAKE_CURRENT_SOURCE_DIR}/hw-protocols/${protoName}-spec.hpp
    COMMAND hyprwire-scanner --client ${path}/${protoName}.xml
            ${CMAKE_CURRENT_SOURCE_DIR}/hw-protocols/
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  target_sources(hyprctl PRIVATE hw-protocols/${protoName}-client.cpp
                                   hw-protocols/${protoName}-client.hpp
                                   hw-protocols/${protoName}-spec.hpp)
endfunction()

hyprprotocol(hw-protocols hyprpaper_core)

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
