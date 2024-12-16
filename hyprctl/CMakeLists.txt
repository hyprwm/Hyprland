cmake_minimum_required(VERSION 3.19)

project(
    hyprctl
    DESCRIPTION "Control utility for Hyprland"
)

pkg_check_modules(hyprctl_deps REQUIRED IMPORTED_TARGET hyprutils>=0.2.4 re2)

add_executable(hyprctl "main.cpp")

target_link_libraries(hyprctl PUBLIC PkgConfig::hyprctl_deps)

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
