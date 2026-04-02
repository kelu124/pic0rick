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
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(pico2-adc-vga C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(pico2-adc-vga main.c )

pico_set_program_name(pico2-adc-vga "pico2-adc-vga")
pico_set_program_version(pico2-adc-vga "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(pico2-adc-vga 0)
pico_enable_stdio_usb(pico2-adc-vga 1)

pico_generate_pio_header(pico2-adc-vga ${CMAKE_CURRENT_LIST_DIR}/adc/adc.pio)
pico_generate_pio_header(pico2-adc-vga ${CMAKE_CURRENT_LIST_DIR}/vga/vga_hsync.pio)
pico_generate_pio_header(pico2-adc-vga ${CMAKE_CURRENT_LIST_DIR}/vga/vga_vsync.pio)
pico_generate_pio_header(pico2-adc-vga ${CMAKE_CURRENT_LIST_DIR}/vga/vga_out.pio)

target_sources(pico2-adc-vga PRIVATE ${CMAKE_CURRENT_LIST_DIR}/main.c ${CMAKE_CURRENT_LIST_DIR}/dac/dac.h ${CMAKE_CURRENT_LIST_DIR}/dac/dac.c ${CMAKE_CURRENT_LIST_DIR}/adc/adc.h ${CMAKE_CURRENT_LIST_DIR}/adc/adc.c ${CMAKE_CURRENT_LIST_DIR}/vga/vga_driver.h ${CMAKE_CURRENT_LIST_DIR}/vga/vga_driver.c ${CMAKE_CURRENT_LIST_DIR}/vga/font.c ${CMAKE_CURRENT_LIST_DIR}/vga/vga.h ${CMAKE_CURRENT_LIST_DIR}/vga/vga.c)

target_link_libraries(pico2-adc-vga PRIVATE pico_stdlib hardware_pio hardware_dma hardware_clocks hardware_gpio hardware_irq pico_multicore)

# Add the standard include files to the build
target_include_directories(pico2-adc-vga PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(pico2-adc-vga)

