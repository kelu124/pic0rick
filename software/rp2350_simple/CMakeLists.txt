cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(~/projets/pico-sdk/pico_sdk_init.cmake)



project(pio_adc_dac)

pico_sdk_init()

# Add arguments
string(TIMESTAMP COMPILE_DATETIME "%Y-%m-%d %H:%M:%S")
# Add to compile definition
add_compile_definitions(BUILD_DATETIME="${COMPILE_DATETIME}")
add_compile_definitions(VERSION_MAJOR=1)
add_compile_definitions(VERSION_MINOR=1)
add_compile_definitions(DEVICE_NAME="pic0rick-simple")


add_executable(pio_adc_dac)

pico_generate_pio_header(pio_adc_dac ${CMAKE_CURRENT_LIST_DIR}/adc/adc.pio)

target_sources(pio_adc_dac PRIVATE ${CMAKE_CURRENT_LIST_DIR}/main.c ${CMAKE_CURRENT_LIST_DIR}/dac/dac.c ${CMAKE_CURRENT_LIST_DIR}/adc/adc.c)

target_link_libraries(pio_adc_dac PRIVATE pico_stdlib hardware_pio hardware_dma hardware_clocks hardware_gpio hardware_irq)
pico_add_extra_outputs(pio_adc_dac)

pico_enable_stdio_usb(pio_adc_dac 1)
pico_enable_stdio_uart(pio_adc_dac 0)