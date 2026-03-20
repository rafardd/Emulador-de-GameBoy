#ifndef CPU_H
#define CPU_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define FLAG_Z (1 << 7)
#define FLAG_N (1 << 6)
#define FLAG_H (1 << 5)
#define FLAG_C (1 << 4)

typedef struct {
    uint8_t A, F;
    union {
        struct {
            uint8_t C, B;
        };
        uint16_t BC;
    };
    union {
        struct {
            uint8_t E, D;
        };
        uint16_t DE;
    };
    union {
        struct {
            uint8_t L, H;
        };
        uint16_t HL;
    };
    uint16_t SP, PC;
} register_map_t;

typedef struct {
    bool is_stopped;
    bool interrupts_enabled;
    bool halt;
    uint8_t joypad_state; // Bits 0-3: Directions, Bits 4-7: Buttons
} cpu_status_t;

// Simple MCB implementation
typedef struct {
    uint8_t rom_bank;
    uint8_t ram_bank;
    bool ram_enabled;
    uint8_t *rom_data;
    uint8_t *ext_ram;
    size_t rom_size;
} mbc_status_t;

extern register_map_t registers;
extern cpu_status_t cpu;
extern mbc_status_t mbc;
extern uint8_t ram[0x10000]; // WRAM, HRAM and registers

// Memory
void write_memory(uint16_t address, uint8_t val);
uint8_t read_memory(uint16_t address);
uint8_t read_next8(uint16_t *pc);
uint16_t read_next16(uint16_t *pc);

// Helpers
void set_flag(uint8_t flag);
void clear_flag(uint8_t flag);
bool is_flag_set(uint8_t flag);
void push_stack(uint16_t val);
uint16_t pop_stack(void);

// ALU
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
