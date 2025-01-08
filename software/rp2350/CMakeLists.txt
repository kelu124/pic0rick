cmake_minimum_required(VERSION 3.13)

include(${CMAKE_CURRENT_LIST_DIR}/../../pico2/pico-sdk/pico_sdk_init.cmake)

project(pio_adc_max)

pico_sdk_init()

add_executable(pio_adc_max)

pico_generate_pio_header(pio_adc_max ${CMAKE_CURRENT_LIST_DIR}/adc/adc.pio)

target_sources(pio_adc_max PRIVATE ${CMAKE_CURRENT_LIST_DIR}/main.c ${CMAKE_CURRENT_LIST_DIR}/dac/dac.c ${CMAKE_CURRENT_LIST_DIR}/adc/adc.c ${CMAKE_CURRENT_LIST_DIR}/max/max14866.c)

target_link_libraries(pio_adc_max PRIVATE pico_stdlib hardware_pio hardware_dma hardware_clocks hardware_gpio hardware_irq)
pico_add_extra_outputs(pio_adc_max)

pico_enable_stdio_usb(pio_adc_max 1)
pico_enable_stdio_uart(pio_adc_max 0)