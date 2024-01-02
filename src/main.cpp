#include <onewire.hpp>

#include <pico/binary_info.h>
#include <pico/stdlib.h>

#include <bitset>
#include <stdio.h>

int main()
{
    bi_decl(bi_program_description("This is a multi-point temperature probe"));

    stdio_init_all();
    printf("PIO: do onewire!\n");

    onewire w0(15, 14);
    auto devices = w0.search();
    for(const auto dev: devices)
    {
        printf("device found: %s\n", std::bitset<64>(dev).to_string().c_str());
    }
}
