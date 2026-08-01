# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if(EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD pico_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(pic0rick-envelope C CXX ASM)
pico_sdk_init()

include(FetchContent)
FetchContent_Declare(
    CMSISDSP
    GIT_REPOSITORY https://github.com/ARM-software/CMSIS-DSP.git
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE
)

set(CMSISCORE
    ${PICO_SDK_PATH}/src/rp2_common/cmsis/stub/CMSIS/Core
    CACHE PATH "CMSIS Core headers" FORCE
)
set(DISABLEFLOAT16 ON CACHE BOOL "Disable CMSIS-DSP float16" FORCE)
FetchContent_MakeAvailable(CMSISDSP)

add_executable(pic0rick-envelope)
pico_set_program_name(pic0rick-envelope "pic0rick-envelope")
pico_set_program_version(pic0rick-envelope "1.0")

# USB is driven directly through TinyUSB so binary frames cannot be mixed with
# Pico SDK stdio output.
pico_enable_stdio_uart(pic0rick-envelope 0)
pico_enable_stdio_usb(pic0rick-envelope 0)

pico_generate_pio_header(pic0rick-envelope
    ${CMAKE_CURRENT_LIST_DIR}/pic0rick/acquisition.pio)
pico_generate_pio_header(pic0rick-envelope
    ${CMAKE_CURRENT_LIST_DIR}/pic0rick/pulser.pio)

target_sources(pic0rick-envelope PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/pic0rick/main.c
    ${CMAKE_CURRENT_LIST_DIR}/pic0rick/acquisition.c
    ${CMAKE_CURRENT_LIST_DIR}/pic0rick/dac.c
    ${CMAKE_CURRENT_LIST_DIR}/pic0rick/dsp.c
    ${CMAKE_CURRENT_LIST_DIR}/pic0rick/pipeline.c
    ${CMAKE_CURRENT_LIST_DIR}/pic0rick/protocol.c
    ${CMAKE_CURRENT_LIST_DIR}/pic0rick/usb_descriptors.c
    ${CMAKE_CURRENT_LIST_DIR}/pic0rick/usb_transport.c
)

target_include_directories(pic0rick-envelope PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/pic0rick
    ${cmsisdsp_SOURCE_DIR}/Include
    ${CMSISCORE}/Include
)

target_compile_definitions(pic0rick-envelope PRIVATE
    ARM_MATH_CM0PLUS
    DISABLEFLOAT16
    U4RK_CMSIS_DSP_VERSION="1.17.0"
)
target_compile_options(pic0rick-envelope PRIVATE
    -O3
    -ffast-math
    -ffunction-sections
    -fdata-sections
)

if(TARGET CMSISDSP)
    target_compile_definitions(CMSISDSP PUBLIC ARM_MATH_CM0PLUS DISABLEFLOAT16)
    target_compile_options(CMSISDSP PRIVATE
        -O3
        -ffast-math
        -ffunction-sections
        -fdata-sections
    )
endif()

target_link_libraries(pic0rick-envelope PRIVATE
    pico_stdlib
    pico_multicore
    pico_util
    hardware_pio
    hardware_dma
    hardware_clocks
    hardware_gpio
    hardware_spi
    tinyusb_device
    CMSISDSP
    m
)

target_link_options(pic0rick-envelope PRIVATE -Wl,--gc-sections)
pico_add_extra_outputs(pic0rick-envelope)

