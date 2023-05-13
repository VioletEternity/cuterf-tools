include_directories(${PNG_INCLUDE_DIRS})

add_executable(nanovna_screenshot nanovna_screenshot.cc common.h)
target_link_libraries(nanovna_screenshot PRIVATE cuterf PNG::PNG)

add_executable(nanovna_extract nanovna_extract.cc)
target_link_libraries(nanovna_extract PRIVATE PNG::PNG)

add_executable(nanovna_data nanovna_data.cc common.h)
target_link_libraries(nanovna_data PRIVATE cuterf)

add_executable(tinysa_screenshot tinysa_screenshot.cc common.h)
target_link_libraries(tinysa_screenshot PRIVATE cuterf PNG::PNG)