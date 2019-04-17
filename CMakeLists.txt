cmake_minimum_required(VERSION 3.5)

project(conjure CXX ASM)

### General Configuration

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED 17)
set(CMAKE_CXX_FLAGS -Wall)
set(CMAKE_CXX_FLAGS_RELEASE -O2)

### Conjure Directories

set(CONJURE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)
set(CONJURE_EXAMPLE_DIR ${CMAKE_SOURCE_DIR}/examples)

set(CONJURE_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/bin)

### the Conjure Library

set(CONJURE_CXX_SRC ${CONJURE_SOURCE_DIR}/conjure/scheduler.cpp)
set(CONJURE_ASM_SRC ${CONJURE_SOURCE_DIR}/conjure/context-switch.S)

set_source_files_properties(
    ${CONJURE_ASM_SRC} PROPERTIES
    COMPILE_FLAGS "-x assembler-with-cpp")

add_library(conjure STATIC ${CONJURE_CXX_SRC} ${CONJURE_ASM_SRC})
set_target_properties(
    conjure PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CONJURE_OUTPUT_DIR}/lib)
target_include_directories(conjure PUBLIC ${CONJURE_SOURCE_DIR})

### Examples

file(GLOB CONJURE_EXAMPLE_SRC ${CONJURE_EXAMPLE_DIR}/*.cpp)

add_custom_target(examples)

foreach(example_file ${CONJURE_EXAMPLE_SRC})
    get_filename_component(example_name ${example_file} NAME_WLE)
    add_executable(${example_name} ${example_file})
    target_link_libraries(${example_name} PRIVATE conjure)
    target_include_directories(${example_name} PRIVATE ${CONJURE_SOURCE_DIR})
    set_target_properties(
        ${example_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CONJURE_OUTPUT_DIR}/examples)
    add_dependencies(examples ${example_name})
endforeach()