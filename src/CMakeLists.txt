pico_add_library(onewire_driver NOFLAG)
target_sources(onewire_driver INTERFACE onewire.c)
target_include_directories(onewire_driver INTERFACE
    ${CMAKE_CURRENT_LIST_DIR})
pico_generate_pio_header(onewire_driver ${CMAKE_CURRENT_LIST_DIR}/onewire.pio)
pico_mirrored_target_link_libraries(onewire_driver INTERFACE
    hardware_pio
    hardware_exception
)
add_executable(picomultipointtemp
    main.cpp
    picopp.cpp
    onewire.cpp
)

target_include_directories(picomultipointtemp PRIVATE
    ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(picomultipointtemp PRIVATE
    onewire_driver
    pico_cyw43_arch_lwip_threadsafe_background
    pico_stdlib
    hardware_pio
    hardware_exception
    project_options
    project_warnings
)

pico_add_extra_outputs(picomultipointtemp)
