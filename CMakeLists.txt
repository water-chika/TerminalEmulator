cmake_minimum_required(VERSION 3.21)

project(TerminalEmulator)

find_package(Vulkan REQUIRED)

add_subdirectory(glfw)

add_executable(terminal_emulator
    terminal_emulator.cpp
    vulkan_render.hpp
    spirv_reader.hpp
    font_loader.hpp
    ${CMAKE_CURRENT_BINARY_DIR}/vertex.spv
    ${CMAKE_CURRENT_BINARY_DIR}/geometry.spv
    ${CMAKE_CURRENT_BINARY_DIR}/fragment.spv
    ${CMAKE_CURRENT_BINARY_DIR}/mesh.spv
)
target_link_libraries(terminal_emulator PUBLIC Vulkan::Vulkan glfw)
set_property(TARGET terminal_emulator PROPERTY CXX_STANDARD 23)

function(compile_glsl stage glsl_file spv_file)
add_custom_command(COMMENT "Compiling ${stage} shader"
                    OUTPUT ${spv_file}
                    COMMAND Vulkan::glslangValidator -V --target-env vulkan1.3 -S ${stage} -o ${spv_file}
                            ${glsl_file}
                    MAIN_DEPENDENCY ${glsl_file}
                    DEPENDS ${glsl_file} Vulkan::glslangValidator)
endfunction()
function(compile_glsl_help stage file_name_without_postfix)
	compile_glsl(${stage}
		${CMAKE_CURRENT_SOURCE_DIR}/${file_name_without_postfix}.glsl
		${CMAKE_CURRENT_BINARY_DIR}/${file_name_without_postfix}.spv
	)
endfunction()

compile_glsl_help(vert vertex)
compile_glsl_help(frag fragment)
compile_glsl_help(geom geometry)
compile_glsl_help(mesh mesh)

add_subdirectory(freetype)

target_link_libraries(terminal_emulator PUBLIC freetype)