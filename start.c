#include <stdint.h>
#include <stdio.h>
#include "instructions.c"
#include <stdbool.h>
#include "cycle_cost.c"

#define FLAG_Z (1 << 7) // If the result of the last operation = 0 , z = 1
#define FLAG_N (1 << 6) // If the last operation was a subtraction, n = 1
#define FLAG_H (1 << 5) // If the operation resulted in a half carry
#define FLAG_C (1 << 4) // If the operation resulted in a Carry out

//  Functions made specifically for the F register
void set_flag(uint8_t flag)
{
    registers.F |= flag;
}
void clear_flag(uint8_t flag)
{
    registers.F &= ~flag;
}
bool is_flag_set(uint8_t flag)
{
    return (registers.F & flag) != 0;
}

struct register_map registers;
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

struct CPU_status CPU;
struct CPU_status
{
    bool is_stopped;
};

uint8_t RAM[0x10000];

void write_memory(uint16_t address, uint8_t val)
{
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
        uint8_t opcode = RAM[registers.PC++];
        int op_cycle_cost = op_cost(opcode);
        frame_cycles += op_cycle_cost;

        switch (opcode)
        {
        case 0x00: // NOP
            nop(&registers.PC);
            break;
        case 0x01: // LD BC, d16
            registers.BC = read_next16(&registers.PC);
            break;
        case 0x02: // LD (BC), A
            write_memory(registers.BC, registers.A);
            break;
        case 0x03: // INC BC
            registers.BC += 1;
            break;
        case 0x04: // INC B
            uint8_t old_value = registers.B;
            registers.B += 1;
            if (registers.B == 0)
            {
                set_flag(FLAG_Z);
            }
            else
            {
                clear_flag(FLAG_Z);
            }
            clear_flag(FLAG_N);
            if ((old_value & 0x0F) == 0x0F)
            {
                set_flag(FLAG_H);
            }
            else
            {
                clear_flag(FLAG_H);
            }
            break;
        case 0x05: // DEC B
            uint8_t old_value = registers.B;
            registers.B -= 1;
            if (registers.B = 0)
            {
                set_flag(FLAG_Z);
            }
            else
            {
                clear_flag(FLAG_Z);
            }
            set_flag(FLAG_N);
            if ((old_value & 0x0F) == 0x00)
            {
                set_flag(FLAG_H);
            }
            else
            {
                clear_flag(FLAG_H);
            }
            break;
        case 0x06:
            ld_reg_8bit(&registers.B, read_next8(&registers.PC));
            break;
        case 0x07:
            registers.A = registers.A << 1;
            uint8_t A7 = registers.A & 1;
            if (A7)
            {
                set_flag(FLAG_C);
            }
            else
            {
                clear_flag(FLAG_C);
            }
            break;
        case 0x08:
            uint16_t address = read_next16(&registers.PC);
            write_memory(address, registers.SP & 0xFF);
            write_memory(address + 1, (registers.SP >> 8) & 0xFF);
            break;
        case 0x09:
            uint16_t old_value = registers.HL;
            ld_reg_16bit(&registers.HL, registers.BC + registers.HL);
            clear_flag(FLAG_N);
            if (check_h_add(old_value, old_value + registers.BC))
            {
                set_flag(FLAG_H);
            }
            else
            {
                clear_flag(FLAG_H);
            }
            if (check_carry(old_value, registers.BC))
            {
                set_flag(FLAG_C);
            }
            else
            {
                clear_flag(FLAG_C);
            }
            break;
        case 0x0A:
            registers.A = read_memory(registers.BC);
            break;
        case 0x0B:
            registers.BC -= 1;
            break;
        case 0x0C:
            registers.C += 1;
            clear_flag(FLAG_Z);
            clear_flag(FLAG_N);
            if ((registers.C - 1 & 0x0F) == 0x0F)
            {
                set_flag(FLAG_H);
            }
            else
            {
                clear_flag(FLAG_H);
            }
            break;
        case 0x0D:
            registers.C -= 1;
            if (registers.C == 0)
            {
                set_flag(FLAG_Z);
            }
            else
            {
                clear_flag(FLAG_Z);
            }
            set_flag(FLAG_N);
            if ((registers.C + 1 & 0x0F) == 0x0F)
            {
                set_flag(FLAG_H);
            }
            else
            {
                clear_flag(FLAG_H);
            }
            break;
        case 0x0E:
            ld_reg_8bit(&registers.C, read_next8(&registers.PC));
            break;
        case 0x0F:
            registers.A = registers.A >> 1;
            if (registers.A & 128)
            {
                set_flag(FLAG_C);
            }
            else
            {
                clear_flag(FLAG_C);
            }
            break;
        case 0x10: // STOP n8
            registers.PC += 1;
            CPU.is_stopped = true;
            break;
        case 0x11: // LD DE, n16
            registers.DE = read_next16(&registers.PC);
            break;
        case 0x12: // LD [DE], A
            RAM[registers.DE] = registers.A;
            break;
        case 0x13: // INC DE
            registers.DE += 1;
            break;
        case 0x14: // INC D 
            old_value = registers.D;
            registers.D += 1;
            if (registers.D == 0)
            {
                set_flag(FLAG_Z);
            }
            else
            {
                clear_flag(FLAG_Z);
            }
            clear_flag(FLAG_N);
            if ((old_value & 0x0F) == 0x0F)
            {
                set_flag(FLAG_H);
            }
            else
            {
                clear_flag(FLAG_H);
            }
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
