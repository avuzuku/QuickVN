cmake_minimum_required(VERSION 3.15)
project(QuickVN VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SFML 3 components
find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System)

# Find QuickJS include directory and library
find_path(QUICKJS_INCLUDE_DIR 
    NAMES quickjs.h 
    PATHS /usr/include /usr/local/include
    PATH_SUFFIXES quickjs
)

find_library(QUICKJS_LIBRARY 
    NAMES quickjs qjs
    PATHS /usr/lib /usr/local/lib
    PATH_SUFFIXES quickjs
)

if(NOT QUICKJS_INCLUDE_DIR OR NOT QUICKJS_LIBRARY)
    message(FATAL_ERROR "QuickJS not found! Please ensure 'quickjs' is installed.")
endif()

message(STATUS "Found QuickJS Include: ${QUICKJS_INCLUDE_DIR}")
message(STATUS "Found QuickJS Library: ${QUICKJS_LIBRARY}")

add_executable(quickjs_vn
    src/main.cpp
    src/Engine.cpp
    src/Engine.hpp
)

target_include_directories(quickjs_vn PRIVATE 
    ${QUICKJS_INCLUDE_DIR}
)

target_link_libraries(quickjs_vn PRIVATE
    SFML::Graphics
    SFML::Window
    SFML::System
    ${QUICKJS_LIBRARY}
    m
    dl
)

# --- Automatic asset synchronization ---
add_custom_target(copy_resources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        ${CMAKE_CURRENT_BINARY_DIR}/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts
        ${CMAKE_CURRENT_BINARY_DIR}/scripts
    COMMENT "Synchronizing assets and scripts to build directory..."
)
add_dependencies(quickjs_vn copy_resources)