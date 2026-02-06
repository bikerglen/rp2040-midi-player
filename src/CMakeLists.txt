cmake_minimum_required(VERSION 3.13)

set(BOARD pico_sdk)
include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(soundboard C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_subdirectory(../../pico-sdk/lib/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src build-fat)
#add_subdirectory(../../pico-extras/src/rp2_common/pico_audio_i2s build-i2s)
#add_subdirectory(../../pico-extras/src/common/pico_audio build-audio)

add_executable(soundboard)

#add_compile_definitions(USE_PRINTF)

target_compile_definitions(soundboard PRIVATE 
  PICO_AUDIO_I2S_MONO_INPUT=0
  PICO_AUDIO_I2S_MONO_OUTPUT=0
	USE_AUDIO_I2S=1
)
target_sources(soundboard PRIVATE main.c hw_config.c)
target_include_directories(soundboard PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_options(soundboard PRIVATE -Xlinker --print-memory-usage)
target_compile_options(soundboard PRIVATE -Wall -Wextra)
target_link_libraries(soundboard 
	pico_stdlib 
	tinyusb_host 
	tinyusb_board 
	no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
  pico_audio_i2s
)

pico_set_program_name(soundboard "MIDI Soundboard")
pico_set_program_version(soundboard "0.1")

pico_enable_stdio_usb(soundboard 0)
pico_enable_stdio_uart(soundboard 1)

pico_add_extra_outputs(soundboard)
