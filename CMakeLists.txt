cmake_minimum_required(VERSION 3.21)

project(TerminalEmulator)

find_package(Vulkan REQUIRED)

set(BOOST_PATH "boost/stage/" CACHE PATH
    "boost library path")
find_package(Boost 1.70 REQUIRED COMPONENTS system PATHS ${BOOST_PATH})

add_subdirectory(glfw)

add_subdirectory(freetype)

add_subdirectory(vulkan_render)

add_executable(terminal_emulator
    terminal_emulator.cpp
)
target_include_directories(
    terminal_emulator
    PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/windows
    )
target_link_libraries(terminal_emulator PUBLIC vulkan_render glfw Boost::system)
set_property(TARGET terminal_emulator PROPERTY CXX_STANDARD 23)

add_executable(sh sh.cpp)
set_property(TARGET sh PROPERTY CXX_STANDARD 23)

add_dependencies(terminal_emulator sh)

#add_executable(attribute_dependence_parser
#    attribute_dependence_parser.cpp
#    lex.yy.c
#    parser.tab.c
#)
#set_property(TARGET attribute_dependence_parser PROPERTY CXX_STANDARD 23)
