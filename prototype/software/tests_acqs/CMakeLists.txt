# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)


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
