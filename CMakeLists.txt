cmake_minimum_required(VERSION 3.18)
project(QuickVN VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QVN_PAK_KEY "quickvn-default-secret-key-2026" CACHE STRING "")
set(QVN_BUILD_STUDIO ON CACHE BOOL "")

if(WIN32 OR ANDROID OR IOS)
    find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System Audio Main)
else()
    find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System Audio)
endif()
find_program(CARGO_EXECUTABLE cargo REQUIRED)

find_path(QUICKJS_INCLUDE_DIR
    NAMES quickjs.h
    HINTS
        ${QUICKJS_ROOT}/include$ENV{QUICKJS_ROOT}/include
    PATHS
        /usr/include
        /usr/local/include
        /opt/homebrew/include
    PATH_SUFFIXES quickjs
)

find_library(QUICKJS_LIBRARY
    NAMES quickjs qjs libquickjs
    HINTS
        ${QUICKJS_ROOT}/lib$ENV{QUICKJS_ROOT}/lib
    PATHS
        /usr/lib
        /usr/local/lib
        /opt/homebrew/lib
    PATH_SUFFIXES quickjs
)

if(NOT QUICKJS_INCLUDE_DIR OR NOT QUICKJS_LIBRARY)
    message(FATAL_ERROR "QuickJS not found. Set -DQUICKJS_ROOT=/path/to/quickjs")
endif()

set(RUST_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/rust_core/Cargo.toml")
set(RUST_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/rust_target")
set(RUST_CARGO_ARGS build --release --manifest-path "${RUST_MANIFEST}" --target-dir "${RUST_TARGET_DIR}")
set(RUST_TARGET_TRIPLE "")

if(ANDROID)
    if(ANDROID_ABI STREQUAL "arm64-v8a")
        set(RUST_TARGET_TRIPLE "aarch64-linux-android")
    elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
        set(RUST_TARGET_TRIPLE "armv7-linux-androideabi")
    elseif(ANDROID_ABI STREQUAL "x86_64")
        set(RUST_TARGET_TRIPLE "x86_64-linux-android")
    elseif(ANDROID_ABI STREQUAL "x86")
        set(RUST_TARGET_TRIPLE "i686-linux-android")
    endif()
elseif(IOS)
    if(CMAKE_OSX_SYSROOT MATCHES "iphonesimulator")
        if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
            set(RUST_TARGET_TRIPLE "x86_64-apple-ios")
        else()
            set(RUST_TARGET_TRIPLE "aarch64-apple-ios-sim")
        endif()
    else()
        set(RUST_TARGET_TRIPLE "aarch64-apple-ios")
    endif()
elseif(APPLE)
    if(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
        set(RUST_TARGET_TRIPLE "x86_64-apple-darwin")
    elseif(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
        set(RUST_TARGET_TRIPLE "aarch64-apple-darwin")
    endif()
elseif(WIN32 AND MINGW AND CMAKE_CROSSCOMPILING)
    set(RUST_TARGET_TRIPLE "x86_64-pc-windows-gnu")
endif()

if(RUST_TARGET_TRIPLE)
    list(APPEND RUST_CARGO_ARGS --target "${RUST_TARGET_TRIPLE}")
    set(RUST_OUT_DIR "${RUST_TARGET_DIR}/${RUST_TARGET_TRIPLE}/release")
else()
    set(RUST_OUT_DIR "${RUST_TARGET_DIR}/release")
endif()

if(MSVC)
    set(RUST_CORE_LIB "${RUST_OUT_DIR}/quickvn_core.lib")
else()
    set(RUST_CORE_LIB "${RUST_OUT_DIR}/libquickvn_core.a")
endif()

file(GLOB RUST_CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/rust_core/src/*.rs")

add_custom_command(
    OUTPUT "${RUST_CORE_LIB}"
    COMMAND "${CARGO_EXECUTABLE}" ${RUST_CARGO_ARGS}
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/rust_core/Cargo.toml"
        ${RUST_CORE_SOURCES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/rust_core"
)

add_custom_target(quickvn_rust_build DEPENDS "${RUST_CORE_LIB}")

set(QUICKVN_PLATFORM_LIBS)
if(WIN32)
    list(APPEND QUICKVN_PLATFORM_LIBS ws2_32 userenv bcrypt ntdll advapi32 synchronization)
elseif(ANDROID)
    list(APPEND QUICKVN_PLATFORM_LIBS android log dl m)
elseif(IOS)
    list(APPEND QUICKVN_PLATFORM_LIBS "-framework Foundation" "-framework Security" m)
elseif(APPLE)
    list(APPEND QUICKVN_PLATFORM_LIBS "-framework Foundation" "-framework Security" m dl)
else()
    list(APPEND QUICKVN_PLATFORM_LIBS m dl pthread)
endif()

set(QUICKVN_SOURCES
    src/main.cpp
    src/core/command.hpp
    src/core/engine.hpp
    src/core/engine.cpp
    src/core/mod_menu.hpp
    src/core/mod_menu.cpp
    src/graphics/scene.hpp
    src/graphics/scene.cpp
    src/audio/audio.hpp
    src/audio/audio.cpp
    src/platform/platform.hpp
    src/platform/platform.cpp
    src/ui/ui.hpp
    src/ui/ui.cpp
    src/script/qjs_bridge.h
    src/script/qjs_bridge.c
    rust_core/include/rust_core.h
)

if(ANDROID)
    add_library(quickjs_vn SHARED ${QUICKVN_SOURCES})
elseif(IOS)
    add_executable(quickjs_vn MACOSX_BUNDLE ${QUICKVN_SOURCES})
else()
    add_executable(quickjs_vn ${QUICKVN_SOURCES})
endif()

add_dependencies(quickjs_vn quickvn_rust_build)
target_compile_definitions(quickjs_vn PRIVATE QVN_PAK_KEY="${QVN_PAK_KEY}")

target_include_directories(quickjs_vn PRIVATE
    src
    rust_core/include
    ${QUICKJS_INCLUDE_DIR}
)

target_link_libraries(quickjs_vn PRIVATE
    SFML::Graphics
    SFML::Window
    SFML::System
    SFML::Audio
    ${QUICKJS_LIBRARY}
    "${RUST_CORE_LIB}"
    ${QUICKVN_PLATFORM_LIBS}
)

if((WIN32 OR ANDROID OR IOS) AND TARGET SFML::Main)
    target_link_libraries(quickjs_vn PRIVATE SFML::Main)
endif()

if(NOT ANDROID AND NOT IOS AND QVN_BUILD_STUDIO)
    add_executable(QuickVN_Studio
        src/tools/launcher_main.cpp
        src/tools/studio_state.hpp
        src/tools/studio_ops.cpp
        src/tools/studio_views.cpp
        src/ui/ui.hpp
        src/ui/ui.cpp
        src/platform/platform.hpp
        src/platform/platform.cpp
        rust_core/include/rust_core.h
    )
    add_dependencies(QuickVN_Studio quickvn_rust_build)
    target_include_directories(QuickVN_Studio PRIVATE src rust_core/include)
    target_link_libraries(QuickVN_Studio PRIVATE
        SFML::Graphics
        SFML::Window
        SFML::System
        "${RUST_CORE_LIB}"
        ${QUICKVN_PLATFORM_LIBS}
    )
endif()