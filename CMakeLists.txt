# GLOBAL - SortNetSAT

# === Includes ===
cmake_policy(SET CMP0069 NEW)
include(cmake/macros.cmake)

# === Configuration ===
cmake_minimum_required(VERSION 3.21)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable multi-processor build
enable_mp()

# Options
set(WARNINGS OFF CACHE BOOL "Enables clang-tidy and warnings on clang-like compilers")
if (WARNINGS)
	add_compile_options("-Wall")
    add_compile_options("-Wextra")
    add_compile_options("-Wpedantic")
    enable_clangtidy()
endif()

set(SNS_MSVC_STATIC_RUNTIME, OFF CACHE BOOL "Use static C runtime library when taregting MSVC ABI")
if (SNS_MSVC_STATIC_RUNTIME)
    use_static_msvc_crt()
endif()

# === Projects ===
project("SortNetSAT")
add_subdirectory("src")