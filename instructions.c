#include "cpu.h"
#include <stdio.h>

void set_flag(uint8_t flag) { registers.F |= flag; }
void clear_flag(uint8_t flag) { registers.F &= ~flag; }
bool is_flag_set(uint8_t flag) { return (registers.F & flag) != 0; }

uint8_t read_memory(uint16_t address)
{
    if (address < 0x4000)
        return MBC.rom_data[address];
    if (address < 0x8000)
    {
        uint32_t real_addr = (uint32_t)(address - 0x4000) + (uint32_t)(MBC.rom_bank * 0x4000);
        if (real_addr >= MBC.rom_size)
            return 0xFF;
        return MBC.rom_data[real_addr];
    }
    if (address >= 0xA000 && address < 0xC000)
    {
        if (MBC.ram_enabled)
            return MBC.ext_ram[(address - 0xA000) + (MBC.ram_bank * 0x2000)];
        return 0xFF;
    }
    if (address >= 0xE000 && address < 0xFE00)
        return RAM[address - 0x2000];

    // Joypad (0xFF00)
    if (address == 0xFF00)
    {
        uint8_t res = RAM[0xFF00] | 0xCF; // Bits 6-7 always 1, 0-3 initially 1 (not pressed)
        if (!(res & 0x10))
        { // Select Directions (Bit 4 is 0)
            res &= ~(CPU.joypad_state & 0x0F);
        }
        if (!(res & 0x20))
        { // Select buttons (Bit 5 is 0)
            res &= ~((CPU.joypad_state >> 4) & 0x0F);
        }
        return res;
    }

    return RAM[address];
}

void write_memory(uint16_t address, uint8_t val)
{
    if (address < 0x2000)
    {
        MBC.ram_enabled = ((val & 0x0F) == 0x0A);
    }
    else if (address < 0x4000)
    {
        MBC.rom_bank = val & 0x7F;
        if (MBC.rom_bank == 0)
            MBC.rom_bank = 1;
    }
    else if (address < 0x6000)
    {
        MBC.ram_bank = val & 0x03;
    }
    else if (address >= 0x8000 && address < 0xA000)
    {
        RAM[address] = val;
    }
    else if (address >= 0xA000 && address < 0xC000)
    {
        if (MBC.ram_enabled)
            MBC.ext_ram[(address - 0xA000) + (MBC.ram_bank * 0x2000)] = val;
    }
    else if (address >= 0xC000 && address < 0xFE00)
    {
        RAM[address] = val;
        if (address < 0xDE00)
            RAM[address + 0x2000] = val;
    }
    else if (address >= 0xFE00 && address < 0xFEA0)
    {
        RAM[address] = val;
    }
    else if (address >= 0xFF00)
    {
        if (address == 0xFF46)
        { // DMA
            uint16_t src = val << 8;
            for (int i = 0; i < 0xA0; i++)
                write_memory(0xFE00 + i, read_memory(src + i));
        }
        // In 0xFF00, only bits 4 and 5 are writable by the game
        if (address == 0xFF00)
            RAM[0xFF00] = (val & 0x30);
        else
            RAM[address] = val;
    }
}

uint8_t read_next8(uint16_t *PC) { return read_memory((*PC)++); }
uint16_t read_next16(uint16_t *PC)
{
    uint16_t lsb = read_next8(PC);
    uint16_t msb = read_next8(PC);
    return (msb << 8) | lsb;
}

void push_stack(uint16_t val)
{
    registers.SP -= 2;
    write_memory(registers.SP, val & 0xFF);
    write_memory(registers.SP + 1, (val >> 8) & 0xFF);
}

uint16_t pop_stack()
{
    uint16_t lsb = read_memory(registers.SP);
    uint16_t msb = read_memory(registers.SP + 1);
    registers.SP += 2;
    return (msb << 8) | lsb;
}

void alu_add(uint8_t val)
{
    uint16_t res_full = (uint16_t)registers.A + (uint16_t)val;
    uint8_t res = (uint8_t)res_full;
    registers.F = 0;
    if (res == 0)
        set_flag(FLAG_Z);
    if (((registers.A & 0xF) + (val & 0xF)) > 0xF)
        set_flag(FLAG_H);
    if (res_full > 0xFF)
        set_flag(FLAG_C);
    registers.A = res;
}

void alu_adc(uint8_t val)
{
    uint8_t c = is_flag_set(FLAG_C) ? 1 : 0;
    uint16_t res_full = (uint16_t)registers.A + (uint16_t)val + (uint16_t)c;
    uint8_t res = (uint8_t)res_full;
    registers.F = 0;
    if (res == 0)
        set_flag(FLAG_Z);
    if (((registers.A & 0xF) + (val & 0xF) + c) > 0xF)
        set_flag(FLAG_H);
    if (res_full > 0xFF)
        set_flag(FLAG_C);
    registers.A = res;
}

void alu_sub(uint8_t val)
{
    uint8_t res = registers.A - val;
    registers.F = FLAG_N;
    if (res == 0)
        set_flag(FLAG_Z);
    if ((registers.A & 0xF) < (val & 0xF))
        set_flag(FLAG_H);
    if (registers.A < val)
        set_flag(FLAG_C);
    registers.A = res;
}

void alu_sbc(uint8_t val)
{
    uint8_t c = is_flag_set(FLAG_C) ? 1 : 0;
    int res_full = (int)registers.A - (int)val - (int)c;
    uint8_t res = (uint8_t)res_full;
    registers.F = FLAG_N;
    if (res == 0)
        set_flag(FLAG_Z);
    if (((int)(registers.A & 0xF) - (int)(val & 0xF) - (int)c) < 0)
        set_flag(FLAG_H);
    if (res_full < 0)
        set_flag(FLAG_C);
    registers.A = res;
}

void alu_and(uint8_t val)
{
    registers.A &= val;
    registers.F = FLAG_H | (registers.A == 0 ? FLAG_Z : 0);
}
void alu_xor(uint8_t val)
{
    registers.A ^= val;
    registers.F = (registers.A == 0 ? FLAG_Z : 0);
}
void alu_or(uint8_t val)
{
    registers.A |= val;
    registers.F = (registers.A == 0 ? FLAG_Z : 0);
}
void alu_cp(uint8_t val)
{
    uint8_t old_a = registers.A;
    alu_sub(val);
    registers.A = old_a;
}

uint8_t alu_inc(uint8_t val)
{
    uint8_t res = val + 1;
    registers.F &= FLAG_C;
    if (res == 0)
        set_flag(FLAG_Z);
    if ((val & 0xF) == 0xF)
        set_flag(FLAG_H);
    return res;
}

uint8_t alu_dec(uint8_t val)
{
    uint8_t res = val - 1;
    registers.F &= FLAG_C;
    set_flag(FLAG_N);
    if (res == 0)
        set_flag(FLAG_Z);
    if ((val & 0xF) == 0)
        set_flag(FLAG_H);
    return res;
}
