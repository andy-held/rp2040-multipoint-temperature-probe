#include <picopp.hpp>

#include <stdexcept>
#include <array>

pico::ProgramInstructions::ProgramInstructions(const pio_program_t *program_in):
    program(program_in)
{
    static_assert(NUM_PIOS == 2, "");
    const std::array<pio_hw_t *, 2> pio_instances{ pio0, pio1 };// try PIO 0 first, as CYW43 prefers PIO 1

    for (auto possible_pio : pio_instances)
    {
        if (pio_can_add_program(possible_pio, program))
        {
            pio = possible_pio;
            pio_memory_offset = pio_add_program(possible_pio, program);
        }
    }
    if (!pio)
    {
        throw std::runtime_error("Could not load PIO program.");
    }
}

pico::Program::Program(const ProgramInstructions &instructions_in):
    instructions(instructions_in)
{
    state_machine_id = (int8_t)pio_claim_unused_sm(instructions.pio, true);
}

pico::Program::~Program()
{
    pio_sm_set_enabled(instructions.pio, state_machine_id, false);
    pio_sm_unclaim(instructions.pio, state_machine_id);
}
