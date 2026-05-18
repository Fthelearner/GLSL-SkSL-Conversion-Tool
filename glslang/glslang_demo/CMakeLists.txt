# Demo: GLSL to SkSL converter
add_executable(glslang-to-sksl glslang_to_sksl.cpp)
target_include_directories(glslang-to-sksl
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/binding_registry/include)
target_link_libraries(glslang-to-sksl PRIVATE glslang glslang-default-resource-limits)

if(UNIX)
    target_link_libraries(glslang-to-sksl PRIVATE pthread)
endif()

set_property(TARGET glslang-to-sksl PROPERTY FOLDER tools)
