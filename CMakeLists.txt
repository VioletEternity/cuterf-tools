cmake_minimum_required(VERSION 3.0)

project(nanovna-tools CXX)

find_package(PNG REQUIRED)

add_definitions(
    -DUNICODE 
    -D_UNICODE
    -D_CRT_SECURE_NO_WARNINGS)

add_library(nanovna libnanovna.h libnanovna.cc)
target_link_libraries(nanovna PRIVATE setupapi)

add_executable(nanovna_screenshot nanovna_screenshot.cc)
target_link_libraries(nanovna_screenshot PRIVATE nanovna PNG::PNG)

add_executable(nanovna_extract nanovna_extract.cc)
target_link_libraries(nanovna_extract PRIVATE PNG::PNG)

add_executable(nanovna_data nanovna_data.cc)
target_link_libraries(nanovna_data PRIVATE nanovna)