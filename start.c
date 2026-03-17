#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "cpu.h"
#include "instructions.c"
#include "cycle_cost.c"
#include "ppu.c"

struct register_map registers;
struct CPU_status CPU;
struct MBC_status MBC;
uint8_t RAM[0x10000];
uint32_t pixel_buffer[WIDTH * HEIGHT];
int frame_cycles = 0;
int timer_counter = 0;
int divider_counter = 0;

void update_timers(int cycles)
{
    // DIV Register (0xFF04) - increments every 256 cycles
    divider_counter += cycles;
    if (divider_counter >= 256)
    {
        divider_counter -= 256;
        RAM[0xFF04]++;
    }

    // Timer Enable (Bit 2 of TAC 0xFF07)
    if (RAM[0xFF07] & 0x04)
    {
        timer_counter += cycles;

        // Timer frequency (Bits 0-1 of TAC)
        int freq = 1024; // Default freq (00)
        switch (RAM[0xFF07] & 0x03)
        {
        case 0:
            freq = 1024;
            break;
        case 1:
            freq = 16;
            break;
        case 2:
            freq = 64;
            break;
        case 3:
            freq = 256;
            break;
        }

        while (timer_counter >= freq)
        {
            timer_counter -= freq;

            if (RAM[0xFF05] == 0xFF)
            {
                // Overflow: reset TIMA to TMA and request Timer Interrupt
                RAM[0xFF05] = RAM[0xFF06];
                RAM[0xFF0F] |= 0x04; // Timer Interrupt (Bit 2)
            }
            else
            {
                RAM[0xFF05]++;
            }
        }
    }
}

void tick(int cycles)
{
    frame_cycles += cycles;
    update_ppu(cycles);
    update_timers(cycles);
}

void init_hw()
{
    registers.PC = 0x0100;
    registers.SP = 0xFFFE;
    registers.A = 0x01;
    registers.F = 0xB0;
    registers.BC = 0x0013;
    registers.DE = 0x00D8;
    registers.HL = 0x014D;

    RAM[0xFF05] = 0x00;
    RAM[0xFF06] = 0x00;
    RAM[0xFF07] = 0x00;
    RAM[0xFF40] = 0x91;
    RAM[0xFF47] = 0xFC;
    RAM[0xFF00] = 0xCF;

    MBC.rom_bank = 1;
    MBC.ram_bank = 0;
    MBC.ram_enabled = false;
    MBC.ext_ram = calloc(1, 0x8000);
    CPU.joypad_state = 0x00;
}

void handle_joypad(SDL_Event *e)
{
    bool down = (e->type == SDL_KEYDOWN);
    uint8_t bit = 0xFF;
    switch (e->key.keysym.sym)
    {
    case SDLK_RIGHT:
        bit = 0;
        break;
    case SDLK_LEFT:
        bit = 1;
        break;
    case SDLK_UP:
        bit = 2;
        break;
    case SDLK_DOWN:
        bit = 3;
        break;
    case SDLK_z:
        bit = 4;
        break; // A
    case SDLK_x:
        bit = 5;
        break; // B
    case SDLK_SPACE:
        bit = 6;
        break; // Select
    case SDLK_RETURN:
        bit = 7;
        break; // Start
    }
    if (bit != 0xFF)
    {
        if (down)
            CPU.joypad_state |= (1 << bit);
        else
            CPU.joypad_state &= ~(1 << bit);
        // Interrupt joypad (Bit 4 IF)
        if (down)
            RAM[0xFF0F] |= 0x10;
    }
}

void handle_interrupts()
{
    uint8_t active = RAM[0xFF0F] & RAM[0xFFFF];
    if (active)
    {
        CPU.halt = false;
        if (CPU.interrupts_enabled)
        {
            for (int i = 0; i < 5; i++)
            {
                if (active & (1 << i))
                {
                    CPU.interrupts_enabled = false;
                    RAM[0xFF0F] &= ~(1 << i);
                    push_stack(registers.PC);
                    registers.PC = 0x40 + (i * 8);
                    tick(20);
                    break;
                }
            }
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
        return printf("Usage: %s {rom}\n", argv[0]);

    FILE *f = fopen(argv[1], "rb");
    if (!f)
        return printf("Error opening ROM\n");
    fseek(f, 0, SEEK_END);
    MBC.rom_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    MBC.rom_data = malloc(MBC.rom_size);
    fread(MBC.rom_data, 1, MBC.rom_size, f);
    fclose(f);

    init_hw();

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        return -1;
    SDL_Window *window = SDL_CreateWindow("Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH * SCALE, HEIGHT * SCALE, SDL_WINDOW_SHOWN);
    SDL_Renderer *renderizador = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture *textura = SDL_CreateTexture(renderizador, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);

    bool running = true;
    SDL_Event event;
    const int CYCLES_PER_FRAME = 70224;
    int instr_count = 0;
    FILE *trace = fopen("trace.log", "w");

    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
                running = false;
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
                handle_joypad(&event);
        }

        handle_interrupts();

        if (!CPU.halt)
        {
            if (instr_count < 50000)
            {
                fprintf(trace, "PC: %04X, A: %02X, F: %02X, BC: %04X, DE: %04X, HL: %04X, SP: %04X\n",
                        registers.PC, registers.A, registers.F, registers.BC, registers.DE, registers.HL, registers.SP);
                instr_count++;
                if (instr_count == 50000)
                    fclose(trace);
            }

            uint8_t opcode = read_next8(&registers.PC);
            int cycles = op_cost(opcode);
            tick(cycles);

            switch (opcode)
            {
            case 0x00:
                break;
            case 0x01:
                registers.BC = read_next16(&registers.PC);
                break;
            case 0x02:
                write_memory(registers.BC, registers.A);
                break;
            case 0x03:
                registers.BC++;
                break;
            case 0x04:
                registers.B = alu_inc(registers.B);
                break;
            case 0x05:
                registers.B = alu_dec(registers.B);
                break;
            case 0x06:
                registers.B = read_next8(&registers.PC);
                break;
            case 0x07:
            {
                uint8_t c = (registers.A & 0x80) >> 7;
                registers.A = (registers.A << 1) | c;
                clear_flag(FLAG_Z | FLAG_N | FLAG_H);
                if (c)
                    set_flag(FLAG_C);
                else
                    clear_flag(FLAG_C);
                break;
            }
            case 0x08:
            {
                uint16_t addr = read_next16(&registers.PC);
                write_memory(addr, registers.SP & 0xFF);
                write_memory(addr + 1, registers.SP >> 8);
                break;
            }
            case 0x09:
            {
                uint16_t old = registers.HL;
                registers.HL += registers.BC;
                clear_flag(FLAG_N);
                if (((old & 0xFFF) + (registers.BC & 0xFFF)) > 0xFFF)
                    set_flag(FLAG_H);
                else
                    clear_flag(FLAG_H);
                if (((uint32_t)old + registers.BC) > 0xFFFF)
                    set_flag(FLAG_C);
                else
                    clear_flag(FLAG_C);
                break;
            }
            case 0x0A:
                registers.A = read_memory(registers.BC);
                break;
            case 0x0B:
                registers.BC--;
                break;
            case 0x0C:
                registers.C = alu_inc(registers.C);
                break;
            case 0x0D:
                registers.C = alu_dec(registers.C);
                break;
            case 0x0E:
                registers.C = read_next8(&registers.PC);
                break;
            case 0x0F:
            {
                uint8_t c = registers.A & 0x01;
                registers.A = (registers.A >> 1) | (c << 7);
                clear_flag(FLAG_Z | FLAG_N | FLAG_H);
                if (c)
                    set_flag(FLAG_C);
                else
                    clear_flag(FLAG_C);
                break;
            }
            case 0x10:
                registers.PC++;
                CPU.is_stopped = true;
                break;
            case 0x11:
                registers.DE = read_next16(&registers.PC);
                break;
            case 0x12:
                write_memory(registers.DE, registers.A);
                break;
            case 0x13:
                registers.DE++;
                break;
            case 0x14:
                registers.D = alu_inc(registers.D);
                break;
            case 0x15:
                registers.D = alu_dec(registers.D);
                break;
            case 0x16:
                registers.D = read_next8(&registers.PC);
                break;
            case 0x17:
            {
                uint8_t oc = is_flag_set(FLAG_C) ? 1 : 0;
                uint8_t nc = (registers.A & 0x80) >> 7;
                registers.A = (registers.A << 1) | oc;
                clear_flag(FLAG_Z | FLAG_N | FLAG_H);
                if (nc)
                    set_flag(FLAG_C);
                else
                    clear_flag(FLAG_C);
                break;
            }
            case 0x18:
            {
                int8_t rel = (int8_t)read_next8(&registers.PC);
                registers.PC += rel;
                break;
            }
            case 0x19:
            {
                uint16_t old = registers.HL;
                registers.HL += registers.DE;
                clear_flag(FLAG_N);
                if (((old & 0xFFF) + (registers.DE & 0xFFF)) > 0xFFF)
                    set_flag(FLAG_H);
                else
                    clear_flag(FLAG_H);
                if (((uint32_t)old + registers.DE) > 0xFFFF)
                    set_flag(FLAG_C);
                else
                    clear_flag(FLAG_C);
                break;
            }
            case 0x1A:
                registers.A = read_memory(registers.DE);
                break;
            case 0x1B:
                registers.DE--;
                break;
            case 0x1C:
                registers.E = alu_inc(registers.E);
                break;
            case 0x1D:
                registers.E = alu_dec(registers.E);
                break;
            case 0x1E:
                registers.E = read_next8(&registers.PC);
                break;
            case 0x1F:
            {
                uint8_t oc = is_flag_set(FLAG_C) ? 1 : 0;
                uint8_t nc = registers.A & 0x01;
                registers.A = (registers.A >> 1) | (oc << 7);
                clear_flag(FLAG_Z | FLAG_N | FLAG_H);
                if (nc)
                    set_flag(FLAG_C);
                else
                    clear_flag(FLAG_C);
                break;
            }
            case 0x20:
            {
                int8_t rel = (int8_t)read_next8(&registers.PC);
                if (!is_flag_set(FLAG_Z))
                {
                    registers.PC += rel;
                    tick(4);
                }
                break;
            }
            case 0x21:
                registers.HL = read_next16(&registers.PC);
                break;
            case 0x22:
                write_memory(registers.HL++, registers.A);
                break;
            case 0x23:
                registers.HL++;
                break;
            case 0x24:
                registers.H = alu_inc(registers.H);
                break;
            case 0x25:
                registers.H = alu_dec(registers.H);
                break;
            case 0x26:
                registers.H = read_next8(&registers.PC);
                break;
            case 0x27:
            {
                if (!is_flag_set(FLAG_N))
                {
                    if (is_flag_set(FLAG_C) || registers.A > 0x99)
                    {
                        registers.A += 0x60;
                        set_flag(FLAG_C);
                    }
                    if (is_flag_set(FLAG_H) || (registers.A & 0x0F) > 0x09)
                    {
                        registers.A += 0x06;
                    }
                }
                else
                {
                    if (is_flag_set(FLAG_C))
                        registers.A -= 0x60;
                    if (is_flag_set(FLAG_H))
                        registers.A -= 0x06;
                }
                if (registers.A == 0)
                    set_flag(FLAG_Z);
                else
                    clear_flag(FLAG_Z);
                clear_flag(FLAG_H);
                break;
            }
            case 0x28:
            {
                int8_t rel = (int8_t)read_next8(&registers.PC);
                if (is_flag_set(FLAG_Z))
                {
                    registers.PC += rel;
                    tick(4);
                }
                break;
            }
            case 0x29:
            {
                uint16_t old = registers.HL;
                registers.HL += registers.HL;
                clear_flag(FLAG_N);
                if (((old & 0xFFF) + (old & 0xFFF)) > 0xFFF)
                    set_flag(FLAG_H);
                else
                    clear_flag(FLAG_H);
                if (((uint32_t)old + old) > 0xFFFF)
                    set_flag(FLAG_C);
                else
                    clear_flag(FLAG_C);
                break;
            }
            case 0x2A:
                registers.A = read_memory(registers.HL++);
                break;
            case 0x2B:
                registers.HL--;
                break;
            case 0x2C:
                registers.L = alu_inc(registers.L);
                break;
            case 0x2D:
                registers.L = alu_dec(registers.L);
                break;
            case 0x2E:
                registers.L = read_next8(&registers.PC);
                break;
            case 0x2F:
                registers.A = ~registers.A;
                set_flag(FLAG_N | FLAG_H);
                break;
            case 0x30:
            {
                int8_t rel = (int8_t)read_next8(&registers.PC);
                if (!is_flag_set(FLAG_C))
                {
                    registers.PC += rel;
                    tick(4);
                }
                break;
            }
            case 0x31:
                registers.SP = read_next16(&registers.PC);
                break;
            case 0x32:
                write_memory(registers.HL--, registers.A);
                break;
            case 0x33:
                registers.SP++;
                break;
            case 0x34:
            {
                uint8_t v = read_memory(registers.HL);
                write_memory(registers.HL, alu_inc(v));
                break;
            }
            case 0x35:
            {
                uint8_t v = read_memory(registers.HL);
                write_memory(registers.HL, alu_dec(v));
                break;
            }
            case 0x36:
                write_memory(registers.HL, read_next8(&registers.PC));
                break;
            case 0x37:
                set_flag(FLAG_C);
                clear_flag(FLAG_N | FLAG_H);
                break;
            case 0x38:
            {
                int8_t rel = (int8_t)read_next8(&registers.PC);
                if (is_flag_set(FLAG_C))
                {
                    registers.PC += rel;
                    tick(4);
                }
                break;
            }
            case 0x39:
            {
                uint16_t old = registers.HL;
                registers.HL += registers.SP;
                clear_flag(FLAG_N);
                if (((old & 0xFFF) + (registers.SP & 0xFFF)) > 0xFFF)
                    set_flag(FLAG_H);
                else
                    clear_flag(FLAG_H);
                if (((uint32_t)old + registers.SP) > 0xFFFF)
                    set_flag(FLAG_C);
                else
                    clear_flag(FLAG_C);
                break;
            }
            case 0x3A:
                registers.A = read_memory(registers.HL--);
                break;
            case 0x3B:
                registers.SP--;
                break;
            case 0x3C:
                registers.A = alu_inc(registers.A);
                break;
            case 0x3D:
                registers.A = alu_dec(registers.A);
                break;
            case 0x3E:
                registers.A = read_next8(&registers.PC);
                break;
            case 0x3F:
                if (is_flag_set(FLAG_C))
                    clear_flag(FLAG_C);
                else
                    set_flag(FLAG_C);
                clear_flag(FLAG_N | FLAG_H);
                break;
            case 0x40 ... 0x75:
            case 0x77 ... 0x7F:
            {
                uint8_t *regs[] = {&registers.B, &registers.C, &registers.D, &registers.E, &registers.H, &registers.L, NULL, &registers.A};
                uint8_t src = (opcode & 0x07);
                uint8_t dst = (opcode >> 3) & 0x07;
                uint8_t val = (src == 6) ? read_memory(registers.HL) : *regs[src];
                if (dst == 6)
                    write_memory(registers.HL, val);
                else
                    *regs[dst] = val;
                break;
            }
            case 0x76:
                CPU.halt = true;
                break;
            case 0x80 ... 0x87:
            {
                uint8_t *regs[] = {&registers.B, &registers.C, &registers.D, &registers.E, &registers.H, &registers.L, NULL, &registers.A};
                alu_add(((opcode & 0x07) == 6) ? read_memory(registers.HL) : *regs[opcode & 0x07]);
                break;
            }
            case 0x88 ... 0x8F:
            {
                uint8_t *regs[] = {&registers.B, &registers.C, &registers.D, &registers.E, &registers.H, &registers.L, NULL, &registers.A};
                alu_adc(((opcode & 0x07) == 6) ? read_memory(registers.HL) : *regs[opcode & 0x07]);
                break;
            }
            case 0x90 ... 0x97:
            {
                uint8_t *regs[] = {&registers.B, &registers.C, &registers.D, &registers.E, &registers.H, &registers.L, NULL, &registers.A};
                alu_sub(((opcode & 0x07) == 6) ? read_memory(registers.HL) : *regs[opcode & 0x07]);
                break;
            }
            case 0x98 ... 0x9F:
            {
                uint8_t *regs[] = {&registers.B, &registers.C, &registers.D, &registers.E, &registers.H, &registers.L, NULL, &registers.A};
                alu_sbc(((opcode & 0x07) == 6) ? read_memory(registers.HL) : *regs[opcode & 0x07]);
                break;
            }
            case 0xA0 ... 0xA7:
            {
                uint8_t *regs[] = {&registers.B, &registers.C, &registers.D, &registers.E, &registers.H, &registers.L, NULL, &registers.A};
                alu_and(((opcode & 0x07) == 6) ? read_memory(registers.HL) : *regs[opcode & 0x07]);
                break;
            }
            case 0xA8 ... 0xAF:
            {
                uint8_t *regs[] = {&registers.B, &registers.C, &registers.D, &registers.E, &registers.H, &registers.L, NULL, &registers.A};
                alu_xor(((opcode & 0x07) == 6) ? read_memory(registers.HL) : *regs[opcode & 0x07]);
                break;
            }
            case 0xB0 ... 0xB7:
            {
                uint8_t *regs[] = {&registers.B, &registers.C, &registers.D, &registers.E, &registers.H, &registers.L, NULL, &registers.A};
                alu_or(((opcode & 0x07) == 6) ? read_memory(registers.HL) : *regs[opcode & 0x07]);
                break;
            }
            case 0xB8 ... 0xBF:
            {
                uint8_t *regs[] = {&registers.B, &registers.C, &registers.D, &registers.E, &registers.H, &registers.L, NULL, &registers.A};
                alu_cp(((opcode & 0x07) == 6) ? read_memory(registers.HL) : *regs[opcode & 0x07]);
                break;
            }
            case 0xC0:
                if (!is_flag_set(FLAG_Z))
                {
                    registers.PC = pop_stack();
                    tick(12);
                }
                break;
            case 0xC1:
                registers.BC = pop_stack();
                break;
            case 0xC2:
            {
                uint16_t addr = read_next16(&registers.PC);
                if (!is_flag_set(FLAG_Z))
                {
                    registers.PC = addr;
                    tick(4);
                }
                break;
            }
            case 0xC3:
                registers.PC = read_next16(&registers.PC);
                break;
            case 0xC4:
            {
                uint16_t addr = read_next16(&registers.PC);
                if (!is_flag_set(FLAG_Z))
                {
                    push_stack(registers.PC);
                    registers.PC = addr;
                    tick(12);
                }
                break;
            }
            case 0xC5:
                push_stack(registers.BC);
                break;
            case 0xC6:
                alu_add(read_next8(&registers.PC));
                break;
            case 0xC7:
                push_stack(registers.PC);
                registers.PC = 0x00;
                break;
            case 0xC8:
                if (is_flag_set(FLAG_Z))
                {
                    registers.PC = pop_stack();
                    tick(12);
                }
                break;
            case 0xC9:
                registers.PC = pop_stack();
                break;
            case 0xCA:
            {
                uint16_t addr = read_next16(&registers.PC);
                if (is_flag_set(FLAG_Z))
                {
                    registers.PC = addr;
                    tick(4);
                }
                break;
            }
            case 0xCB:
            {
                uint8_t cb = read_next8(&registers.PC);
                tick(op_cost_cb(cb));
                uint8_t *regs[] = {&registers.B, &registers.C, &registers.D, &registers.E, &registers.H, &registers.L, NULL, &registers.A};
                uint8_t v = ((cb & 0x07) == 6) ? read_memory(registers.HL) : *regs[cb & 0x07];
                if (cb < 0x40)
                {
                    uint8_t oc = is_flag_set(FLAG_C) ? 1 : 0;
                    uint8_t nc = 0;
                    switch (cb >> 3)
                    {
                    case 0:
                        nc = v >> 7;
                        v = (v << 1) | nc;
                        break;
                    case 1:
                        nc = v & 1;
                        v = (v >> 1) | (nc << 7);
                        break;
                    case 2:
                        nc = v >> 7;
                        v = (v << 1) | oc;
                        break;
                    case 3:
                        nc = v & 1;
                        v = (v >> 1) | (oc << 7);
                        break;
                    case 4:
                        nc = v >> 7;
                        v <<= 1;
                        break;
                    case 5:
                        nc = v & 1;
                        v = (uint8_t)((int8_t)v >> 1);
                        break;
                    case 6:
                        v = (v << 4) | (v >> 4);
                        nc = 0;
                        break;
                    case 7:
                        nc = v & 1;
                        v >>= 1;
                        break;
                    }
                    if (v == 0)
                        set_flag(FLAG_Z);
                    else
                        clear_flag(FLAG_Z);
                    clear_flag(FLAG_N | FLAG_H);
                    if (nc)
                        set_flag(FLAG_C);
                    else
                        clear_flag(FLAG_C);
                    if ((cb >> 3) == 6)
                        clear_flag(FLAG_C);
                }
                else if (cb < 0x80)
                {
                    if (!(v & (1 << ((cb >> 3) & 7))))
                        set_flag(FLAG_Z);
                    else
                        clear_flag(FLAG_Z);
                    clear_flag(FLAG_N);
                    set_flag(FLAG_H);
                }
                else if (cb < 0xC0)
                    v &= ~(1 << ((cb >> 3) & 7));
                else
                    v |= (1 << ((cb >> 3) & 7));
                if (((cb & 0x07) == 6) && (cb >= 0x80 || cb < 0x40))
                    write_memory(registers.HL, v);
                else if (((cb & 0x07) != 6) && (cb >= 0x80 || cb < 0x40))
                    *regs[cb & 0x07] = v;
                break;
            }
            case 0xCC:
            {
                uint16_t addr = read_next16(&registers.PC);
                if (is_flag_set(FLAG_Z))
                {
                    push_stack(registers.PC);
                    registers.PC = addr;
                    tick(12);
                }
                break;
            }
            case 0xCD:
            {
                uint16_t addr = read_next16(&registers.PC);
                push_stack(registers.PC);
                registers.PC = addr;
                break;
            }
            case 0xCE:
                alu_adc(read_next8(&registers.PC));
                break;
            case 0xCF:
                push_stack(registers.PC);
                registers.PC = 0x08;
                break;
            case 0xD0:
                if (!is_flag_set(FLAG_C))
                {
                    registers.PC = pop_stack();
                    tick(12);
                }
                break;
            case 0xD1:
                registers.DE = pop_stack();
                break;
            case 0xD2:
            {
                uint16_t addr = read_next16(&registers.PC);
                if (!is_flag_set(FLAG_C))
                {
                    registers.PC = addr;
                    tick(4);
                }
                break;
            }
            case 0xD4:
            {
                uint16_t addr = read_next16(&registers.PC);
                if (!is_flag_set(FLAG_C))
                {
                    push_stack(registers.PC);
                    registers.PC = addr;
                    tick(12);
                }
                break;
            }
            case 0xD5:
                push_stack(registers.DE);
                break;
            case 0xD6:
                alu_sub(read_next8(&registers.PC));
                break;
            case 0xD7:
                push_stack(registers.PC);
                registers.PC = 0x10;
                break;
            case 0xD8:
                if (is_flag_set(FLAG_C))
                {
                    registers.PC = pop_stack();
                    tick(12);
                }
                break;
            case 0xD9:
                registers.PC = pop_stack();
                CPU.interrupts_enabled = true;
                break;
            case 0xDA:
            {
                uint16_t addr = read_next16(&registers.PC);
                if (is_flag_set(FLAG_C))
                {
                    registers.PC = addr;
                    tick(4);
                }
                break;
            }
            case 0xDC:
            {
                uint16_t addr = read_next16(&registers.PC);
                if (is_flag_set(FLAG_C))
                {
                    push_stack(registers.PC);
                    registers.PC = addr;
                    tick(12);
                }
                break;
            }
            case 0xDE:
                alu_sbc(read_next8(&registers.PC));
                break;
            case 0xDF:
                push_stack(registers.PC);
                registers.PC = 0x18;
                break;
            case 0xE0:
                write_memory(0xFF00 + read_next8(&registers.PC), registers.A);
                break;
            case 0xE1:
                registers.HL = pop_stack();
                break;
            case 0xE2:
                write_memory(0xFF00 + registers.C, registers.A);
                break;
            case 0xE5:
                push_stack(registers.HL);
                break;
            case 0xE6:
                alu_and(read_next8(&registers.PC));
                break;
            case 0xE7:
                push_stack(registers.PC);
                registers.PC = 0x20;
                break;
            case 0xE8:
            {
                int8_t rel = (int8_t)read_next8(&registers.PC);
                uint16_t old = registers.SP;
                registers.SP += rel;
                clear_flag(FLAG_Z | FLAG_N);
                if (((old & 0xF) + (rel & 0xF)) > 0xF)
                    set_flag(FLAG_H);
                else
                    clear_flag(FLAG_H);
                if (((old & 0xFF) + (rel & 0xFF)) > 0xFF)
                    set_flag(FLAG_C);
                else
                    clear_flag(FLAG_C);
                break;
            }
            case 0xE9:
                registers.PC = registers.HL;
                break;
            case 0xEA:
                write_memory(read_next16(&registers.PC), registers.A);
                break;
            case 0xEE:
                alu_xor(read_next8(&registers.PC));
                break;
            case 0xEF:
                push_stack(registers.PC);
                registers.PC = 0x28;
                break;
            case 0xF0:
                registers.A = read_memory(0xFF00 + read_next8(&registers.PC));
                break;
            case 0xF1:
            {
                uint16_t v = pop_stack();
                registers.A = v >> 8;
                registers.F = v & 0xF0;
                break;
            }
            case 0xF2:
                registers.A = read_memory(0xFF00 + registers.C);
                break;
            case 0xF3:
                CPU.interrupts_enabled = false;
                break;
            case 0xF5:
                push_stack((registers.A << 8) | registers.F);
                break;
            case 0xF6:
                alu_or(read_next8(&registers.PC));
                break;
            case 0xF7:
                push_stack(registers.PC);
                registers.PC = 0x30;
                break;
            case 0xF8:
            {
                int8_t rel = (int8_t)read_next8(&registers.PC);
                uint16_t old = registers.SP;
                registers.HL = registers.SP + rel;
                clear_flag(FLAG_Z | FLAG_N);
                if (((old & 0xF) + (rel & 0xF)) > 0xF)
                    set_flag(FLAG_H);
                else
                    clear_flag(FLAG_H);
                if (((old & 0xFF) + (rel & 0xFF)) > 0xFF)
                    set_flag(FLAG_C);
                else
                    clear_flag(FLAG_C);
                break;
            }
            case 0xF9:
                registers.SP = registers.HL;
                break;
            case 0xFA:
                registers.A = read_memory(read_next16(&registers.PC));
                break;
            case 0xFB:
                CPU.interrupts_enabled = true;
                break;
            case 0xFE:
                alu_cp(read_next8(&registers.PC));
                break;
            case 0xFF:
                push_stack(registers.PC);
                registers.PC = 0x38;
                break;
            default:
                printf("ERRO: Opcode desconhecido: 0x%02X no PC: 0x%04X\n", opcode, registers.PC - 1);
                exit(1);
            }
        }
        else
        {
            tick(4);
        }

        if (frame_cycles >= CYCLES_PER_FRAME)
        {
            frame_cycles -= CYCLES_PER_FRAME;
            SDL_UpdateTexture(textura, NULL, pixel_buffer, WIDTH * 4);
            SDL_RenderClear(renderizador);
            SDL_RenderCopy(renderizador, textura, NULL, NULL);
            SDL_RenderPresent(renderizador);
        }
    }

    free(MBC.rom_data);
    free(MBC.ext_ram);
    SDL_DestroyTexture(textura);
    SDL_DestroyRenderer(renderizador);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
