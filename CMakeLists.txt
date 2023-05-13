cmake_minimum_required(VERSION 3.0)

project(nanovna-tools CXX)

find_package(PNG REQUIRED)

add_definitions(
    -DUNICODE 
    -D_UNICODE
    -D_CRT_SECURE_NO_WARNINGS)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    
add_subdirectory(src/libcuterf)
add_subdirectory(src/tools)