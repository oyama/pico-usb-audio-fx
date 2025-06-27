cmake_minimum_required(VERSION 3.13...3.27)

include(pico_sdk_import.cmake)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
project(pico-usb-audio-fx C CXX ASM)
pico_sdk_init()

add_executable(${CMAKE_PROJECT_NAME}
  src/main.c
  src/usb_descriptors.c
  src/led.c
)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/fx_tapestop.c)
#target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/fx_lpf.c)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(${CMAKE_PROJECT_NAME}
  pico_stdlib
  tinyusb_device
  tinyusb_board
)
pico_add_extra_outputs(${CMAKE_PROJECT_NAME})
