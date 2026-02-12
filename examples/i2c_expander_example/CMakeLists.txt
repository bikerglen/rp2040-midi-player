cmake_minimum_required(VERSION 3.13)

set(BOARD pico_sdk)
include(pico_sdk_import.cmake)

project(i2c_example C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(i2c_example)

target_sources(i2c_example PRIVATE main.c)
target_include_directories(i2c_example PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_options(i2c_example PRIVATE -Xlinker --print-memory-usage)
target_compile_options(i2c_example PRIVATE -Wall -Wextra)
target_link_libraries(i2c_example pico_stdlib hardware_i2c)

pico_enable_stdio_usb(i2c_example 0)
pico_enable_stdio_uart(i2c_example 1)

target_include_directories(i2c_example PRIVATE
 ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(i2c_example)

