add_library(sdio
  sdio.pio
  sdio.h sdio.cpp
  sd_card_sdio.cpp
)

pico_generate_pio_header(sdio ${CMAKE_CURRENT_LIST_DIR}/sdio.pio)

target_link_libraries(sdio PRIVATE hardware_pio
                           PRIVATE hardware_dma
                           PUBLIC sdfat
)

target_include_directories(sdio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

include_directories(${PROJECT_SOURCE_DIR})
