pico_add_library(onewire_pio NOFLAG)
target_sources(onewire_pio INTERFACE onewire.c)
target_include_directories(onewire_pio INTERFACE
    ${CMAKE_CURRENT_LIST_DIR})
pico_generate_pio_header(onewire_pio ${CMAKE_CURRENT_LIST_DIR}/onewire.pio)
pico_mirrored_target_link_libraries(onewire_pio INTERFACE
    hardware_pio
    hardware_exception
)
