#include <onewire.h>
#include <onewire.pio.h>

#include "hardware/clocks.h"
#include "hardware/sync.h"
#include "hardware/dma.h"

/* Configure a PIO/sm for onewire */
/* Note on pinctlz: GPIO pins have pulldown after reset. This enables
   the external transistor initially and causes some glitches during
   pin configuraion. Add external pullup to +3V3 (e.g. 12k). */
/* TODO: We could implement an option by which pinctlz is disabled.
   We do not need to change the PIO code, just avoid anything that
   configures pinctlz in the PIO mapping. */
void onewire_program_init(
     PIO pio,       /* pio object (pio0/pio1) */
     uint sm,       /* state machine number */
     uint offset,   /* onewire code offset in PIO instr. memory */
     uint pin,      /* Pin number for 1-Wire data signal */
     uint pinctlz   /* Pin number for external FET strong pullup */
)
{
    pio_sm_config c = onewire_program_get_default_config(offset);
    sm_config_set_out_pins(&c, pinctlz, 1);
    sm_config_set_set_pins(&c, pinctlz, 1);
    sm_config_set_in_pins(&c, pin);
    sm_config_set_sideset_pins(&c, pin);
    uint div = clock_get_hz(clk_sys)/1000000 * 3;
    sm_config_set_clkdiv_int_frac(&c, div, 0);
    sm_config_set_out_shift(&c, true, true, 8);
    sm_config_set_in_shift(&c, true, true, 8);

    gpio_init(pin);
    gpio_set_dir(pin, 0);
    gpio_pull_up(pin);

    gpio_init(pinctlz);
    gpio_put(pinctlz, 1);
    gpio_set_dir(pinctlz, 1);

    //pio_sm_set_pins_with_mask(pio, sm, 1<<pin, 1<<pin);
    //pio_sm_set_pindirs_with_mask(pio, sm, 1<<pin, 1<<pin);

    pio_gpio_init(pio, pin);
    //   gpio_set_oeover(pin, GPIO_OVERRIDE_INVERT); // see above
    pio_sm_set_pins_with_mask(pio, sm, 0, 1<<pin);

    pio_gpio_init(pio, pinctlz);
    pio_sm_set_pins_with_mask(pio, sm, 1<<pinctlz, 1<<pinctlz);
    pio_sm_set_pindirs_with_mask(pio, sm, 1<<pinctlz, 1<<pinctlz);

    /* Preload register y with 1 to keep pinctlz = high when
       state machine starts running */
    pio_sm_exec(pio, sm, pio_encode_set(pio_y, 1));

    pio_sm_init(pio, sm, offset + onewire_offset_start, &c);
}
