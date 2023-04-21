cmake_minimum_required(VERSION 3.0)

project(nanovna-tools CXX)

find_package(PNG REQUIRED)

add_definitions(
    -DUNICODE 
    -D_UNICODE
    -D_CRT_SECURE_NO_WARNINGS)

add_executable(nanovna_screenshot nanovna_screenshot.cc)
target_link_libraries(nanovna_screenshot PRIVATE setupapi PNG::PNG)