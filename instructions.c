#include "cpu.h"

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
    return RAM[(*PC)++];
}
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
    if (res == 0)
        set_flag(FLAG_Z);
    else
        clear_flag(FLAG_Z);
    clear_flag(FLAG_N);
    if (((registers.A & 0xF) + (val & 0xF)) > 0xF)
        set_flag(FLAG_H);
    else
        clear_flag(FLAG_H);
    if (res_full > 0xFF)
        set_flag(FLAG_C);
    else
        clear_flag(FLAG_C);
    registers.A = res;
}

void alu_adc(uint8_t val)
{
    uint8_t c = is_flag_set(FLAG_C) ? 1 : 0;
    uint16_t res_full = (uint16_t)registers.A + (uint16_t)val + (uint16_t)c;
    uint8_t res = (uint8_t)res_full;
    if (res == 0)
        set_flag(FLAG_Z);
    else
        clear_flag(FLAG_Z);
    clear_flag(FLAG_N);
    if (((registers.A & 0xF) + (val & 0xF) + c) > 0xF)
        set_flag(FLAG_H);
    else
        clear_flag(FLAG_H);
    if (res_full > 0xFF)
        set_flag(FLAG_C);
    else
        clear_flag(FLAG_C);
    registers.A = res;
}

void alu_sub(uint8_t val)
{
    uint8_t res = registers.A - val;
    if (res == 0)
        set_flag(FLAG_Z);
    else
        clear_flag(FLAG_Z);
    set_flag(FLAG_N);
    if ((registers.A & 0xF) < (val & 0xF))
        set_flag(FLAG_H);
    else
        clear_flag(FLAG_H);
    if (registers.A < val)
        set_flag(FLAG_C);
    else
        clear_flag(FLAG_C);
    registers.A = res;
}

void alu_sbc(uint8_t val)
{
    uint8_t c = is_flag_set(FLAG_C) ? 1 : 0;
    int res_full = (int)registers.A - (int)val - (int)c;
    uint8_t res = (uint8_t)res_full;
    if (res == 0)
        set_flag(FLAG_Z);
    else
        clear_flag(FLAG_Z);
    set_flag(FLAG_N);
    if (((int)(registers.A & 0xF) - (int)(val & 0xF) - (int)c) < 0)
        set_flag(FLAG_H);
    else
        clear_flag(FLAG_H);
    if (res_full < 0)
        set_flag(FLAG_C);
    else
        clear_flag(FLAG_C);
    registers.A = res;
}

void alu_and(uint8_t val)
{
    registers.A &= val;
    if (registers.A == 0)
        set_flag(FLAG_Z);
    else
        clear_flag(FLAG_Z);
    clear_flag(FLAG_N);
    set_flag(FLAG_H);
    clear_flag(FLAG_C);
}

void alu_xor(uint8_t val)
{
    registers.A ^= val;
    if (registers.A == 0)
        set_flag(FLAG_Z);
    else
        clear_flag(FLAG_Z);
    clear_flag(FLAG_N | FLAG_H | FLAG_C);
}

void alu_or(uint8_t val)
{
    registers.A |= val;
    if (registers.A == 0)
        set_flag(FLAG_Z);
    else
        clear_flag(FLAG_Z);
    clear_flag(FLAG_N | FLAG_H | FLAG_C);
}

void alu_cp(uint8_t val)
{
    uint8_t res = registers.A - val;
    if (res == 0)
        set_flag(FLAG_Z);
    else
        clear_flag(FLAG_Z);
    set_flag(FLAG_N);
    if ((registers.A & 0xF) < (val & 0xF))
        set_flag(FLAG_H);
    else
        clear_flag(FLAG_H);
    if (registers.A < val)
        set_flag(FLAG_C);
    else
        clear_flag(FLAG_C);
}

uint8_t alu_inc(uint8_t val)
{
    uint8_t res = val + 1;
    if (res == 0)
        set_flag(FLAG_Z);
    else
        clear_flag(FLAG_Z);
    clear_flag(FLAG_N);
    if ((val & 0xF) == 0xF)
        set_flag(FLAG_H);
    else
        clear_flag(FLAG_H);
    return res;
}

uint8_t alu_dec(uint8_t val)
{
    uint8_t res = val - 1;
    if (res == 0)
        set_flag(FLAG_Z);
    else
        clear_flag(FLAG_Z);
    set_flag(FLAG_N);
    if ((val & 0xF) == 0)
        set_flag(FLAG_H);
    else
        clear_flag(FLAG_H);
    return res;
}
