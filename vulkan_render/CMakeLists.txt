
add_executable(generate_spirv_reader_include generate_spirv_reader_include.cpp)
set_property(TARGET generate_spirv_reader_include PROPERTY CXX_STANDARD 23)
add_custom_command(COMMENT "Generating spirv_reader_os.hpp"
                    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/include/spirv_reader_os.hpp
                    COMMAND generate_spirv_reader_include ${CMAKE_CURRENT_BINARY_DIR}/include/spirv_reader_os.hpp
                    DEPENDS generate_spirv_reader_include)

add_library(vulkan_render
    vulkan_render.cpp
    vulkan_render.hpp
    spirv_reader.hpp
    ${CMAKE_CURRENT_BINARY_DIR}/include/shader_path.hpp
    ${CMAKE_CURRENT_BINARY_DIR}/include/spirv_reader_os.hpp
    ${CMAKE_CURRENT_BINARY_DIR}/vertex.spv
    ${CMAKE_CURRENT_BINARY_DIR}/geometry.spv
    ${CMAKE_CURRENT_BINARY_DIR}/fragment.spv
    ${CMAKE_CURRENT_BINARY_DIR}/mesh.spv
    ${CMAKE_CURRENT_BINARY_DIR}/task.spv
)
target_include_directories(
    vulkan_render
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/include)

target_link_libraries(vulkan_render PUBLIC Vulkan::Vulkan freetype)
set_property(TARGET vulkan_render PROPERTY CXX_STANDARD 23)

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
    set(
        ${file_name_without_postfix}_shader_path
        "${CMAKE_CURRENT_BINARY_DIR}/${file_name_without_postfix}.spv"
        PARENT_SCOPE
    )
endfunction()

compile_glsl_help(vert vertex)
compile_glsl_help(frag fragment)
compile_glsl_help(geom geometry)
compile_glsl_help(mesh mesh)
compile_glsl_help(task task)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/template/shader_path.hpp
    ${CMAKE_CURRENT_BINARY_DIR}/include/shader_path.hpp
)