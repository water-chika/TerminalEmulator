cmake_minimum_required(VERSION 3.20)

project(TerminalEmulator)

find_package(Vulkan REQUIRED)

add_subdirectory(glfw)

add_executable(terminal_emulator terminal_emulator.cpp spirv_reader.hpp)
target_link_libraries(terminal_emulator PUBLIC Vulkan::Vulkan glfw)
set_property(TARGET terminal_emulator PROPERTY CXX_STANDARD 23)