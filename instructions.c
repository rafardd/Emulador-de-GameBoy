#include <stdint.h>

// This file is a collection of functions for the main file.

void nop(uint16_t *PC)
{
    *PC = (*PC) + 1;
}
void ld_reg_reg(uint8_t *destiny, uint8_t origin)
{
    *destiny = origin;
}
void ld_reg_8bit(uint8_t *destiny, uint8_t val)
{
    *destiny = val;
}
void ld_reg_16bit(uint16_t *destiny, uint16_t val)
{
    *destiny = val;
}
void ld_reg_regval16(uint16_t *destiny, uint16_t val)
{
    *destiny = RAM;
}
