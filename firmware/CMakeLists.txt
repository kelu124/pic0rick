cmake_minimum_required(VERSION 3.13)

# ---- Board: must be pico2_w for WiFi on RP2350 ----
set(PICO_BOARD pico2_w)

# ---- WiFi credentials (override with: cmake -DWIFI_SSID="..." -DWIFI_PASSWORD="...") ----
if(NOT DEFINED WIFI_SSID)
    set(WIFI_SSID "YOUR_WIFI_SSID")
endif()
if(NOT DEFINED WIFI_PASSWORD)
    set(WIFI_PASSWORD "YOUR_WIFI_PASSWORD")
endif()

include(pico_sdk_import.cmake)

project(rp2350_daq C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# ---- Executable ----
add_executable(${PROJECT_NAME}
    main.c
    adc/adc.c
    max/max14866.c
    wifi/wifi_server.c
)

# ---- PIO generation (must be after add_executable) ----
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/adc/adc.pio)
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/max/max14866.pio)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}               # for lwipopts.h
    ${CMAKE_CURRENT_LIST_DIR}/adc
    ${CMAKE_CURRENT_LIST_DIR}/max
    ${CMAKE_CURRENT_LIST_DIR}/wifi
)

# Pass WiFi credentials as compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIFI_SSID=\"${WIFI_SSID}\"
    WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
    CYW43_HOST_NAME="rp2350-daq"
)

target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
    hardware_pio
    hardware_dma
    hardware_clocks
    hardware_gpio
)

# ---- USB serial output ----
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

# ---- Generate UF2 ----
pico_add_extra_outputs(${PROJECT_NAME})