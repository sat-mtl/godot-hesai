cmake_minimum_required(VERSION 3.17)

# Silence unused variable warning when specified from toolchain
if(CMAKE_C_COMPILER)
endif()

set(LIBNAME "godot-hesai" CACHE STRING "The name of the library")
set(GODOT_PROJECT_DIR "demo" CACHE STRING "The directory of a Godot project folder")

find_package(Python3 3.4 REQUIRED)
find_program(GIT git REQUIRED)

# ── godot-cpp submodule ──────────────────────────────────────────────────────
if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/godot-cpp/src")
    message(NOTICE "godot-cpp submodule not found, initializing...")
    execute_process(
        COMMAND git submodule update --init godot-cpp
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()
add_subdirectory(godot-cpp SYSTEM)

# ── HesaiLidar SDK ───────────────────────────────────────────────────────────
# Disable the sample executables and optional features we don't need.
set(DISENABLE_TEST_CC ON CACHE BOOL "" FORCE)
set(WITH_PTCS_USE     OFF CACHE BOOL "" FORCE)
set(FIND_CUDA         OFF CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/HesaiLidar_SDK_2.0 SYSTEM)

# ── Project ──────────────────────────────────────────────────────────────────
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${godot-cpp_SOURCE_DIR}/cmake")
include(GodotCPPModule)

get_target_property(GODOTCPP_SUFFIX   godot::cpp GODOTCPP_SUFFIX)
get_target_property(GODOTCPP_PLATFORM godot::cpp GODOTCPP_PLATFORM)

project(godot-hesai
    VERSION 1.0
    LANGUAGES CXX
)

add_library(${LIBNAME} SHARED)

target_sources(${LIBNAME}
    PRIVATE
    src/register_types.cpp
    src/register_types.h
    src/hesai_lidar.cpp
    src/hesai_lidar.hpp
)

target_link_libraries(${LIBNAME} PRIVATE godot-cpp hesai_sdk_lib)

set_property(TARGET ${LIBNAME} PROPERTY CXX_STANDARD 17)

set_target_properties(${LIBNAME}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "$<1:${PROJECT_SOURCE_DIR}/bin/${GODOTCPP_PLATFORM}>"
    PREFIX ""
    OUTPUT_NAME "${LIBNAME}${GODOTCPP_SUFFIX}"
)

if(LINUX)
    set_target_properties(${LIBNAME} PROPERTIES
        LINK_FLAGS "-Wl,-rpath,./bin/linux -Wl,-rpath,./ -Wl,-rpath,./addons/godot-hesai/bin/linux")
endif()

# ── Post-build: copy into demo project ──────────────────────────────────────
set(GODOT_PROJECT_BINARY_DIR "${PROJECT_SOURCE_DIR}/${GODOT_PROJECT_DIR}/bin/${GODOTCPP_PLATFORM}")
set(BINARY_DIR "${PROJECT_SOURCE_DIR}/bin/${GODOTCPP_PLATFORM}")

add_custom_command(TARGET ${LIBNAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GODOT_PROJECT_BINARY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:${LIBNAME}>"
        "${GODOT_PROJECT_BINARY_DIR}/$<TARGET_FILE_NAME:${LIBNAME}>"
)
