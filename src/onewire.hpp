#pragma once

#include <picopp.hpp>

#include <hardware/pio.h>
#include <pico/stdlib.h>

#include <cstdint>
#include <optional>
#include <tuple>
#include <vector>

uint8_t calc_crc8(const uint8_t* data, const size_t size);

class onewire
{
  public:
    onewire(uint8_t pin, uint8_t pinctlz);

    int reset() const;

    /* Transmit a byte */
    void transmit(uint8_t byte) const;

    /* Receive a byte */
    uint8_t receive() const;

    /*  Transmit a byte and activate strong pullup after
        last bit has been sent.
        Note: onewire_tx_byte_spu returns when the rx fifo
        has been read. This is 50 us prior to the end of the bit
        and hence 50 us prior to the strong pullup actually
        activated.
        Either consider this when controlling the strong
        pullup time or wait for idle before taking time. */
    void transmit_then_pull_up(uint8_t byte) const;

    /* Reset the strong pullup (set pinctlz to high) */
    void disable_pull_up() const;

    using search_state = std::tuple<uint64_t, int8_t>;
    /**
     * @brief Incrementally search new devices by passing the last discrepancy
     * point to the next iteration. This does not work with hot-swapping devices.
     * When a new device is connected, a new search has to be started, i.e.,
     * last_discrepancy has to be 0. When this function returns no values, the
     * search has to be started from scratch, too.
     *
     * @param last_discrepancy The last discrepancy of the previous iteration.
     *     Initialize with 0.
     * @return new device id, new last discrepancy
     */
    std::optional<search_state> incremental_search(const search_state& state) const;

    //--------------------------------------------------------------------------
    // Do a general search. Continues from the previous search state.
    // The search state can be reset by using the 'OWFirst' function.
    //
    // Returns:   TRUE (1) : when a 1-Wire device was found and its
    //                       Serial Number placed in the global ROM
    //            FALSE (0): when no new device was found.  Either the
    //                       last search was the last device or there
    //                       are no devices on the 1-Wire Net.
    //
    std::vector<uint64_t> search() const;

  private:
    void set_fifo_thresh(uint thresh) const;
    void set_timing(uint usecs) const;
    void wait_until_sm_idle() const;
    uint8_t transmit_or_receive_bits(const uint8_t bits = 8, const uint8_t data = 0xff) const;

    pico::Program program;
    uint8_t pin; /* Pin number for 1-Wire data signal */
    uint8_t pinctlz; /* Pin number for external FET strong pullup */
};
