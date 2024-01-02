#pragma once

#include <hardware/structs/pio.h>
#include <hardware/pio.h>

namespace pico
{
// Associates a pio_program_t with a PIO
struct ProgramInstructions
{
    ProgramInstructions(const pio_program_t *program);

    ~ProgramInstructions()
    {
        pio_remove_program(pio, program, pio_memory_offset);
    }

    const pio_program_t* program;
    PIO pio = nullptr;
    uint pio_memory_offset;
};

// Loads a program on a state machine.
struct Program
{
    Program(const ProgramInstructions &instructions);
    ~Program();
    const ProgramInstructions& instructions;
    uint state_machine_id;
};
}// namespace pico
