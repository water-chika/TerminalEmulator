cmake_minimum_required(VERSION 3.21)

project(TerminalEmulator)

find_package(Vulkan REQUIRED)

find_package(Boost 1.70 REQUIRED COMPONENTS system PATHS ${CMAKE_CURRENT_SOURCE_DIR}/boost_1_79_0/stage/lib/cmake/)

add_subdirectory(glfw)

add_subdirectory(freetype)

add_subdirectory(vulkan_render)

add_executable(terminal_emulator
    terminal_emulator.cpp
)
target_include_directories(
    terminal_emulator
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/include)
target_link_libraries(terminal_emulator PUBLIC vulkan_render glfw Boost::system)
set_property(TARGET terminal_emulator PROPERTY CXX_STANDARD 23)

add_executable(sh sh.cpp)

add_dependencies(terminal_emulator sh)
