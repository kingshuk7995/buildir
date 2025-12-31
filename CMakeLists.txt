cmake_minimum_required(VERSION 3.20)

project(
    buildir
    VERSION 0.1.0
    DESCRIPTION "A make-like build tool"
    LANGUAGES CXX
)

# C++ Standard configuration
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default build type if none provided
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING
        "Choose the type of build."
        FORCE
    )
endif()

# Target
add_executable(buildir
    src/main.cpp
    src/file_reader.cpp
    src/parse.cpp
    src/exec.cpp
    src/process_pool.cpp
)

target_include_directories(buildir
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Warnings (compiler-aware)
target_compile_options(buildir
    PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wshadow
        >
        $<$<CXX_COMPILER_ID:MSVC>:
            /W4
            /permissive-
        >
)

# Debug / Release specifics
target_compile_definitions(buildir
    PRIVATE
        $<$<CONFIG:Debug>:DEBUG>
)

# optimization defaults
target_compile_options(buildir
    PRIVATE
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:RelWithDebInfo>:-O2 -g>
)

# Link options (future-proof)
if(UNIX AND NOT APPLE)
    target_link_options(buildir PRIVATE -pthread)
endif()
