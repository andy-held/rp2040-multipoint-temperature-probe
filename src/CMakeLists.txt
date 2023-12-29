add_subdirectory(onewire_pio)

add_executable(picomultipointtemp
    main.cpp
    picopp.cpp
    onewire.cpp
)

target_include_directories(picomultipointtemp PRIVATE
    ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(picomultipointtemp PRIVATE
    onewire_pio
    pico_cyw43_arch_lwip_threadsafe_background
    pico_stdlib
    hardware_pio
    hardware_exception
    project_options
    project_warnings
)

pico_add_extra_outputs(picomultipointtemp)
