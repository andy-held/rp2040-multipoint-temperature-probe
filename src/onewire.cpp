#include <onewire.hpp>

#include <onewirepio.hpp>
#include <picopp.hpp>

#include <hardware/clocks.h>
#include <hardware/pio.h>
#include <hardware/structs/pio.h>
#include <pico/types.h>

#include <stdexcept>

namespace
{
constexpr const uint8_t ONEWIRE_SEARCH_COMMAND = 0xf0;

constexpr const int TIMEOUT_RETRIES = 2000;
constexpr const int CHECKSUM_RETRIES = 10;

const pico::ProgramInstructions &get_onewire_instructions()
{
    static const pico::ProgramInstructions onewire_instructions(&onewire_program);
    return onewire_instructions;
}

uint8_t calc_crc8(const uint8_t* data, const size_t size)
{
    // See Application Note 27
    uint8_t crc8 = 0;
    for (size_t j = 0; j < size; j++)
    {
        crc8 = crc8 ^ data[j];
        for (int i = 0; i < 8; ++i)
        {
            if (crc8 & 1)
                crc8 = (crc8 >> 1) ^ 0x8c;
            else
                crc8 = (crc8 >> 1);
        }
    }

    return crc8;
}
}// namespace

onewire::onewire(uint8_t pin_in, uint8_t pinctlz_in)
    : program(get_onewire_instructions()), pin(pin_in), pinctlz(pinctlz_in)
{
    auto pio = program.instructions.pio;
    auto state_machine = program.state_machine_id;
    auto memory_offset = program.instructions.pio_memory_offset;
    pio_sm_config config = onewire_program_get_default_config(memory_offset);
    sm_config_set_out_pins(&config, pinctlz, 1);
    sm_config_set_set_pins(&config, pinctlz, 1);
    sm_config_set_in_pins(&config, pin);
    sm_config_set_sideset_pins(&config, pin);
    uint div = clock_get_hz(clk_sys) / 1e6 * 3;
    sm_config_set_clkdiv_int_frac(&config, div, 0);
    sm_config_set_out_shift(&config, true, true, 8);
    sm_config_set_in_shift(&config, true, true, 8);

    gpio_init(pin);
    gpio_set_dir(pin, 0);
    gpio_pull_up(pin);

    gpio_init(pinctlz);
    gpio_put(pinctlz, 1);
    gpio_set_dir(pinctlz, 1);

    // pio_sm_set_pins_with_mask(pio, sm, 1<<pin, 1<<pin);
    // pio_sm_set_pindirs_with_mask(pio, sm, 1<<pin, 1<<pin);

    pio_gpio_init(pio, pin);
    //   gpio_set_oeover(pin, GPIO_OVERRIDE_INVERT); // see above
    pio_sm_set_pins_with_mask(pio, state_machine, 0, 1 << pin);

    pio_gpio_init(pio, pinctlz);
    pio_sm_set_pins_with_mask(pio, state_machine, 1 << pinctlz, 1 << pinctlz);
    pio_sm_set_pindirs_with_mask(pio, state_machine, 1 << pinctlz, 1 << pinctlz);

    /* Preload register y with 1 to keep pinctlz = high when
       state machine starts running */
    pio_sm_exec(pio, state_machine, pio_encode_set(pio_y, 1));

    pio_sm_init(pio, state_machine, memory_offset + onewire_offset_start, &config);
    pio_sm_set_enabled(pio, state_machine, true);
}

void onewire::set_fifo_thresh(uint thresh)
{
    auto pio = program.instructions.pio;
    auto state_machine = program.state_machine_id;

    if (thresh >= 32) { thresh = 0; }

    uint old = pio->sm[state_machine].shiftctrl;
    old &= PIO_SM0_SHIFTCTRL_PUSH_THRESH_BITS | PIO_SM0_SHIFTCTRL_PULL_THRESH_BITS;

    uint new_thresh = ((thresh & 0x1fu) << PIO_SM0_SHIFTCTRL_PUSH_THRESH_LSB)
          | ((thresh & 0x1fu) << PIO_SM0_SHIFTCTRL_PULL_THRESH_LSB);

    if (old != new_thresh)
    {
        uint need_restart = 0;
        if (pio->ctrl & (1u << state_machine))
        {
            /* If state machine is enabled, it must be disabled
               and restarted when we change fifo thresholds,
               or evil things happen */

            /* When we attempt fifo threshold switching, we assume
               that all fifo operations have been done and hence
               all bits have been almost processed, but the
               state machine might not have reached the wating state
               as it still does some delays to ensure timing for
               the very last bit (Similar for reset).
               Just wait for the 'wating' state to be reached */
            wait_until_sm_idle();

            pio_sm_set_enabled(pio, state_machine, false);
            need_restart = 1;
        }

        hw_write_masked(
            &pio->sm[state_machine].shiftctrl, new_thresh, PIO_SM0_SHIFTCTRL_PUSH_THRESH_BITS | PIO_SM0_SHIFTCTRL_PULL_THRESH_BITS);

        if (need_restart)
        {
            pio_sm_restart(pio, state_machine);
            pio_sm_set_enabled(pio, state_machine, true);
        }
    }
}

int onewire::reset()
{
    auto pio = program.instructions.pio;
    auto state_machine = program.state_machine_id;
    auto memory_offset = program.instructions.pio_memory_offset;

    /* Switch to slow timing for reset */
    set_timing(70);
    set_fifo_thresh(1);

    // onewire_do_reset(pio, sm, offset);
    pio_sm_exec(pio, state_machine, pio_encode_jmp(memory_offset + onewire_offset_reset));
    while (pio_sm_get_rx_fifo_level(pio, state_machine) == 0)
    {} /* wait */;
    int ret = ((pio_sm_get(pio, state_machine) & 0x80000000) == 0);

    /* when rx fifo has filled we still need to wait for
       the remaineder of the reset to execute before we
       can manipulate the clkdiv.
       Just wait until we reach the waiting state */
    wait_until_sm_idle();

    /* Restore normal timing */
    set_timing(3);

    return ret;// 1=detected, 0=not
}

void onewire::set_timing(uint usecs)
{
    auto pio = program.instructions.pio;
    auto state_machine = program.state_machine_id;

    uint div = clock_get_hz(clk_sys) / 1e6 * usecs;
    pio_sm_set_clkdiv_int_frac(pio, state_machine, div, 0);
    pio_sm_clkdiv_restart(pio, state_machine);
}

/* Wait for idle state to be reached. This is only
   useful when you know that all but the last bit
   have been processed (after having checked fifos) */
void onewire::wait_until_sm_idle()
{
    auto pio = program.instructions.pio;
    auto state_machine = program.state_machine_id;
    auto memory_offset = program.instructions.pio_memory_offset;

    uint8_t waiting_addr = memory_offset + onewire_offset_waiting;
    auto retries = TIMEOUT_RETRIES;
    while (pio_sm_get_pc(pio, state_machine) != waiting_addr)
    {
        sleep_us(1);
        if (retries-- < 0)
        {
            /* FIXME: do something clever in case of
               timeout */
        }
    }
}

uint8_t onewire::transmit_or_receive_bits(const uint8_t bits, const uint8_t data)
{
    auto pio = program.instructions.pio;
    auto state_machine = program.state_machine_id;

    set_fifo_thresh(bits);
    pio->txf[state_machine] = data;
    while (pio_sm_get_rx_fifo_level(pio, state_machine) == 0)
    {} /* wait */;
    /* Returned byte is in 31..24 of RX fifo! */
    return (pio_sm_get(pio, state_machine) >> (32-bits)) & 0xff;
}

void onewire::transmit(uint8_t byte)
{
    transmit_or_receive_bits(8, byte);
}

uint8_t onewire::receive()
{
    return transmit_or_receive_bits();
}

void onewire::transmit_then_pull_up(uint8_t byte)
{
    auto pio = program.instructions.pio;
    auto state_machine = program.state_machine_id;

    transmit_or_receive_bits(7, byte);

    set_fifo_thresh(1);
    pio_sm_exec(pio, state_machine, pio_encode_set(pio_y, 0));
    pio->txf[state_machine] = byte >> 7;
    while (pio_sm_get_rx_fifo_level(pio, state_machine) == 0)
    {} /* wait */;
    pio_sm_get(pio, state_machine); /* read to drain RX fifo */
}

void onewire::disable_pull_up()
{
    auto pio = program.instructions.pio;
    auto state_machine = program.state_machine_id;

    /* Preset y register so no SPU during next bit */
    pio_sm_exec(pio, state_machine, pio_encode_set(pio_y, 1));
    /* Set pinctlz pin to high ! */
    pio_sm_exec(pio, state_machine, pio_encode_set(pio_pins, 1));
}

std::optional<onewire::search_state> onewire::incremental_search(const onewire::search_state& state)
{

    const auto [last_device_id, most_significant_discrepancy] = state;

    if(!reset())
    {
        return {};
    }

    transmit(ONEWIRE_SEARCH_COMMAND);

    uint8_t search_direction = 0;

    uint64_t device_id = 0;
    int8_t discrepancy = 64;
    for (int8_t bit_id = 0; bit_id < 64; bit_id++)
    {
        // get 2 bits
        uint8_t id_bits = transmit_or_receive_bits(2, 0b11); // [complementary id bit, id bit]
        search_direction = id_bits & 0b1; // we initially go down the direction of the last id bit
        switch (id_bits)
        {
        case 0b00: // discrepancy
        {
            bool last_discrepancy_reached = bit_id == most_significant_discrepancy; // we hit the last discrepancy, go down the other way
            bool bit_in_last_device_id_is_one = // before the most significant discrepancy, follow the path of the last device id
                (bit_id < most_significant_discrepancy) && ((last_device_id >> bit_id) & 0b1);
            if(last_discrepancy_reached || bit_in_last_device_id_is_one)
            {
                search_direction = 1;
                break;
            }
            else
            {
                discrepancy = bit_id; // we hit a discrepancy and are going down the 0-direction, *insert NOTED-meme*
            }
            break;
        }
        case 0b11: // no devices left
            return {};
        default:
            // either 0b01 (all device ids are 1 at the current bit) or 0b10 (all 0)
            // all remaining devices agree on the last bit, continue in the set search_direction
            break;
        }

        transmit_or_receive_bits(1, search_direction);

        device_id += uint64_t(search_direction) << bit_id;
    }
    return {search_state{device_id, discrepancy}};
}

std::vector<uint64_t> onewire::search()
{
    std::vector<uint64_t> device_ids;

    int8_t most_significant_discrepancy = -1;
    uint64_t last_device_id = 0;
    int checksum_fails = 0;
    while (most_significant_discrepancy != 64)
    {
        auto search_result = incremental_search({last_device_id, most_significant_discrepancy});
        if(!search_result.has_value())
        {
            return device_ids;
        }
        auto [device_id, discrepancy] = search_result.value();
        if(calc_crc8((uint8_t*)&device_id, sizeof(decltype(device_id))))
        {
            // checksum is invalid, something went wrong, try again
            printf("checksum of device %llu invalid\n", device_id);
            checksum_fails++;
            if(checksum_fails > CHECKSUM_RETRIES)
            {
                throw std::runtime_error("Max checksum fails exceeded.");
            }
            continue;
        }
        last_device_id = device_id;
        most_significant_discrepancy = discrepancy;
        device_ids.push_back(device_id);
    }

    return device_ids;
}
