cmake_minimum_required(VERSION 3.19)

project(
    hyprctl
    DESCRIPTION "Control utility for Hyprland"
)

add_executable(hyprctl "main.cpp")

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
