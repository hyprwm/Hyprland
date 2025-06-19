# Tests CMakeLists.txt
cmake_minimum_required(VERSION 3.19)

# Find Google Test
find_package(GTest REQUIRED)

# Enable testing
enable_testing()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/render
    ${CMAKE_SOURCE_DIR}/src/helpers
)

# Render batching tests
add_executable(test_render_batching
    render/test_render_batching.cpp
)

target_link_libraries(test_render_batching
    GTest::GTest
    GTest::Main
    pthread
)

# Add test
add_test(NAME RenderBatchingTest COMMAND test_render_batching)

# Set C++ standard
set_target_properties(test_render_batching PROPERTIES
    CXX_STANDARD 26
    CXX_STANDARD_REQUIRED ON
)