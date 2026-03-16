#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdbool.h>

#define FLAG_Z (1 << 7)
#define FLAG_N (1 << 6)
#define FLAG_H (1 << 5)
#define FLAG_C (1 << 4)

struct register_map
{
    uint8_t A;
    uint8_t F;
    union
    {
        struct
        {
            uint8_t C;
            uint8_t B;
        } bc_reg;
        uint16_t BC;
    };
    union
    {
        struct
        {
            uint8_t E;
            uint8_t D;
        } de_reg;
        uint16_t DE;
    };
    union
    {
        struct
        {
            uint8_t L;
            uint8_t H;
        } hl_reg;
        uint16_t HL;
    };
    uint16_t SP;
    uint16_t PC;
};

// Add these macros to keep the old syntax working (registers.B, registers.C, etc.)
#define B bc_reg.B
#define C bc_reg.C
#define D de_reg.D
#define E de_reg.E
#define H hl_reg.H
#define L hl_reg.L

struct CPU_status
{
    bool is_stopped;
    bool interrupts_enabled;
};

extern struct register_map registers;
extern struct CPU_status CPU;
extern uint8_t RAM[0x10000];

// Memory/IO helpers
void write_memory(uint16_t address, uint8_t val);
uint8_t read_memory(uint16_t address);
uint8_t read_next8(uint16_t *PC);
uint16_t read_next16(uint16_t *PC);

// Flag helpers
void set_flag(uint8_t flag);
void clear_flag(uint8_t flag);
bool is_flag_set(uint8_t flag);

// Stack helpers
void push_stack(uint16_t val);
uint16_t pop_stack();

// ALU helpers
void alu_add(uint8_t val);
void alu_adc(uint8_t val);
void alu_sub(uint8_t val);
void alu_sbc(uint8_t val);
void alu_and(uint8_t val);
void alu_xor(uint8_t val);
void alu_or(uint8_t val);
void alu_cp(uint8_t val);
uint8_t alu_inc(uint8_t val);
uint8_t alu_dec(uint8_t val);

#endif
