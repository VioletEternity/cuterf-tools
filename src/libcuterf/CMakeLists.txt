add_library(cuterf
    include/cuterf.h
    nanovna.cc
    tinysa.cc
    serial.h
    serial.cc)
target_include_directories(cuterf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(cuterf PRIVATE setupapi)
