add_executable(nanovna_screenshot nanovna_screenshot.cc)
target_link_libraries(nanovna_screenshot PRIVATE cuterf PNG::PNG)

add_executable(nanovna_extract nanovna_extract.cc)
target_link_libraries(nanovna_extract PRIVATE PNG::PNG)

add_executable(nanovna_data nanovna_data.cc)
target_link_libraries(nanovna_data PRIVATE cuterf)