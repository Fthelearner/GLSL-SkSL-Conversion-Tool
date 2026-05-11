cmake_minimum_required(VERSION 3.20)

project(binding_registry LANGUAGES CXX)

# Use OBJECT library to avoid install export issues
add_library(binding_registry OBJECT
    src/builtin_bindings.cpp
    src/intrinsic_bindings.cpp
    src/feature_bindings.cpp
    src/provenance.cpp
)

target_compile_features(binding_registry PUBLIC cxx_std_20)

target_include_directories(binding_registry
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(binding_registry
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

set_property(TARGET binding_registry PROPERTY POSITION_INDEPENDENT_CODE ON)
