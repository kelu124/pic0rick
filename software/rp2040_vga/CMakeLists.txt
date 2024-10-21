cmake_minimum_required(VERSION 3.13)

include(~/pico/pico-sdk/pico_sdk_init.cmake)

project(pio_adc_dac_vga ASM C CXX)

pico_sdk_init()

add_executable(pio_adc_dac_vga)

pico_generate_pio_header(pio_adc_dac_vga ${CMAKE_CURRENT_LIST_DIR}/adc/adc.pio)
pico_generate_pio_header(pio_adc_dac_vga ${CMAKE_CURRENT_LIST_DIR}/vga/vga_hsync.pio)
pico_generate_pio_header(pio_adc_dac_vga ${CMAKE_CURRENT_LIST_DIR}/vga/vga_vsync.pio)
pico_generate_pio_header(pio_adc_dac_vga ${CMAKE_CURRENT_LIST_DIR}/vga/vga_out.pio)

target_sources(pio_adc_dac_vga PRIVATE ${CMAKE_CURRENT_LIST_DIR}/main.c ${CMAKE_CURRENT_LIST_DIR}/dac/dac.h ${CMAKE_CURRENT_LIST_DIR}/dac/dac.c ${CMAKE_CURRENT_LIST_DIR}/adc/adc.h ${CMAKE_CURRENT_LIST_DIR}/adc/adc.c ${CMAKE_CURRENT_LIST_DIR}/vga/vga_driver.h ${CMAKE_CURRENT_LIST_DIR}/vga/vga_driver.c ${CMAKE_CURRENT_LIST_DIR}/vga/font.c ${CMAKE_CURRENT_LIST_DIR}/vga/vga.h ${CMAKE_CURRENT_LIST_DIR}/vga/vga.c)

target_link_libraries(pio_adc_dac_vga PRIVATE pico_stdlib hardware_pio hardware_dma hardware_clocks hardware_gpio hardware_irq pico_multicore)
pico_add_extra_outputs(pio_adc_dac_vga)

pico_enable_stdio_usb(pio_adc_dac_vga 1)
pico_enable_stdio_uart(pio_adc_dac_vga 0)
