cmake_minimum_required(VERSION 3.21)

project(TerminalEmulator)

find_package(Vulkan REQUIRED)

add_subdirectory(glfw)

add_executable(terminal_emulator
    terminal_emulator.cpp spirv_reader.hpp
    ${CMAKE_CURRENT_BINARY_DIR}/vertex.spv
    ${CMAKE_CURRENT_BINARY_DIR}/fragment.spv)
target_link_libraries(terminal_emulator PUBLIC Vulkan::Vulkan glfw)
set_property(TARGET terminal_emulator PROPERTY CXX_STANDARD 23)

function(compile_glsl stage glsl_file spv_file)
add_custom_command(COMMENT "Compiling ${stage} shader"
                    OUTPUT ${spv_file}
                    COMMAND Vulkan::glslangValidator -V -S ${stage} -o ${spv_file}
                            ${glsl_file}
                    MAIN_DEPENDENCY ${glsl_file}
                    DEPENDS ${glsl_file} Vulkan::glslangValidator)
endfunction()

compile_glsl(vert ${CMAKE_CURRENT_SOURCE_DIR}/vertex.glsl ${CMAKE_CURRENT_BINARY_DIR}/vertex.spv)
compile_glsl(frag ${CMAKE_CURRENT_SOURCE_DIR}/fragment.glsl ${CMAKE_CURRENT_BINARY_DIR}/fragment.spv)