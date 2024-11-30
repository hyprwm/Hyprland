cmake_minimum_required(VERSION 3.19)

project(
    hyprpm
    DESCRIPTION "A Hyprland Plugin Manager"
)

file(GLOB_RECURSE SRCFILES CONFIGURE_DEPENDS "src/*.cpp")

set(CMAKE_CXX_STANDARD 23)

pkg_check_modules(deps REQUIRED IMPORTED_TARGET tomlplusplus hyprutils>=0.2.4)

add_executable(hyprpm ${SRCFILES})

target_link_libraries(hyprpm PUBLIC PkgConfig::hyprpm_deps)

# binary
install(TARGETS hyprpm)

# shell completions
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/hyprpm.bash
        DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
        RENAME hyprpm)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/hyprpm.fish
        DESTINATION ${CMAKE_INSTALL_DATADIR}/fish/vendor_completions.d)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/hyprpm.zsh
        DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions
        RENAME _hyprpm)
