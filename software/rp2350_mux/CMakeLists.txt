# Generated Cmake Pico project file 

cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(~/projets/pico-sdk/pico_sdk_init.cmake)

project(adc-pulse C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()


# Add arguments
string(TIMESTAMP COMPILE_DATETIME "%Y-%m-%d %H:%M:%S")
# Add to compile definition
add_compile_definitions(BUILD_DATETIME="${COMPILE_DATETIME}")
add_compile_definitions(VERSION_MAJOR=1)
add_compile_definitions(VERSION_MINOR=1)
add_compile_definitions(DEVICE_NAME="pic0rick-mux")

# Add executable. Default name is the project name, version 0.1

add_executable(adc-pulse)

pico_set_program_name(adc-pulse "adc-pulse")
pico_set_program_version(adc-pulse "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(adc-pulse 0)
pico_enable_stdio_usb(adc-pulse 1)

pico_generate_pio_header(adc-pulse ${CMAKE_CURRENT_LIST_DIR}/adc/adc.pio)
pico_generate_pio_header(adc-pulse ${CMAKE_CURRENT_LIST_DIR}/max/max14866.pio)

target_sources(adc-pulse PRIVATE ${CMAKE_CURRENT_LIST_DIR}/main.c ${CMAKE_CURRENT_LIST_DIR}/dac/dac.c ${CMAKE_CURRENT_LIST_DIR}/adc/adc.c ${CMAKE_CURRENT_LIST_DIR}/max/max14866.c)

# Add the standard include files to the build
target_include_directories(adc-pulse PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(adc-pulse PRIVATE pico_stdlib hardware_pio hardware_dma hardware_clocks hardware_gpio hardware_irq)


pico_add_extra_outputs(adc-pulse)

