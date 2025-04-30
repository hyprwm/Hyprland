cmake_minimum_required(VERSION 3.19)

project(
    hyprpm
    DESCRIPTION "A Hyprland Plugin Manager"
)

file(GLOB_RECURSE SRCFILES CONFIGURE_DEPENDS "src/*.cpp")

set(CMAKE_CXX_STANDARD 23)

pkg_check_modules(hyprpm_deps REQUIRED IMPORTED_TARGET tomlplusplus hyprutils>=0.7.0)

find_package(glaze QUIET)
if (NOT glaze_FOUND)
    set(GLAZE_VERSION v4.2.3)
    message(STATUS "glaze dependency not found, retrieving ${GLAZE_VERSION} with FetchContent")
    include(FetchContent)
    FetchContent_Declare(
        glaze
        GIT_REPOSITORY https://github.com/stephenberry/glaze.git
        GIT_TAG ${GLAZE_VERSION}
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(glaze)
endif()

add_executable(hyprpm ${SRCFILES})

target_link_libraries(hyprpm PUBLIC PkgConfig::hyprpm_deps glaze::glaze)

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
