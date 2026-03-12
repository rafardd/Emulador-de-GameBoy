#include <stdint.h>
#include <stdio.h>
#include "instructions.c"

struct register_map
{
    uint8_t A; // Accumulator
    uint8_t F; // Flags
    union
    {
        struct
        {
            uint8_t C;
            uint8_t B;
        };
        uint16_t BC;
    };
    union
    {
        struct
        {
            uint8_t E;
            uint8_t D;
        };
        uint16_t DE;
    };
    union
    {
        struct
        {
            uint8_t L;
            uint8_t H;
        };
        uint16_t HL;
    };
    uint16_t SP; // Stack Pointer
    uint16_t PC; // Program Counter
};

uint8_t RAM[0x10000];

void write_memory(uint16_t address, uint8_t val)
{
    // Simplified: For now, we allow writing anywhere in RAM
    RAM[address] = val;
}

uint8_t read_memory(uint16_t address)
{
    return RAM[address];
}

uint8_t read_next8(uint16_t *PC)
{
    uint8_t val = RAM[*PC];
    *PC = (*PC) + 1;
    return val;
}

uint16_t read_next16(uint16_t *PC)
{
    uint16_t lsb = read_next8(PC);
    uint16_t msb = read_next8(PC);
    return (msb << 8) | lsb;
}

int op_cost(uint8_t opcode)
{
    switch (opcode)
    {
    case 0x00:
        return 1;
    case 0x01:
        return 3;
    case 0x02:
        return 2;
    case 0x03:
        return 2;
    case 0x04:
        return 1;
    case 0x05:
        return 1;
    default:
        return 1;
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Incorrect number of rom names provided \n Usage: %s {rom name}\n", argv[0]);
        return 0;
    }

    FILE *rom = fopen(argv[1], "rb");
    if (rom == NULL)
    {
        printf("An error occurred when trying to open the ROM\n");
        return 0;
    }

    // Read the ROM into the beginning of RAM
    fread(RAM, 1, 0x8000, rom);
    fclose(rom);

    // Starting all the registers, like the boot sequence in gameboy
    struct register_map registers;
    registers.PC = 0x0100; // Initializing the PC register where the game starts
    registers.SP = 0xFFFE; // top of the stack pile
    registers.A = 0x01;
    registers.F = 0xB0;
    registers.BC = 0x0013;
    registers.DE = 0x00D8;
    registers.HL = 0x014D;

    const int CYCLES_PER_FRAME = 69905;
    int frame_cycles = 0;

    // Start Running the game
    printf("Starting emulation...\n");
    while (1)
    {
        uint8_t opcode = RAM[registers.PC];
        int op_cycle_cost = op_cost(opcode);
        frame_cycles += op_cycle_cost;

        switch (opcode)
        {
        case 0x00: // NOP
            nop(&registers.PC);
            break;
        case 0x01:          // LD BC, d16
            registers.PC++; // Skip opcode
            registers.BC = read_next16(&registers.PC);
            break;
        case 0x02: // LD (BC), A
            write_memory(registers.BC, registers.A);
            registers.PC++;
            break;
        case 0x03: // INC BC
            registers.BC += 1;
            registers.PC++;
            break;
        case 0x04: // INC B
            registers.B += 1;
            registers.PC++;
            break;
        case 0x05: // DEC B
            registers.B -= 1;
            registers.PC++;
            break;

        default:
            printf("Unknown opcode: 0x%02X at PC: 0x%04X\n", opcode, registers.PC);
            return 1;
        }

        if (frame_cycles >= CYCLES_PER_FRAME)
        {
            frame_cycles = 0;
            // Aqui futuramente você chamaria a atualização da tela (GPU/PPU)
        }
    }

    return 0;
}
