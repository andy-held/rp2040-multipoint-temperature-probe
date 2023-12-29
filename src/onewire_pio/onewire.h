#pragma once

#include <hardware/gpio.h>
#include <hardware/pio.h>

void onewire_program_init(
     PIO pio,       /* pio object (pio0/pio1) */
     uint sm,       /* state machine number */
     uint offset,   /* onewire code offset in PIO instr. memory */
     uint pin,      /* Pin number for 1-Wire data signal */
     uint pinctlz   /* Pin number for external FET strong pullup */
);
