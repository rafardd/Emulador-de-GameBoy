#include <stdint.h>
#include <stdbool.h>

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
// Function for flag F
// a = original value , b = what was added
bool check_h_add_8bit(uint8_t a, uint8_t b)
{
    // If the sum of the two halfs > 0x0F then H flag should be set
    return (((a & 0x0F) + (b & 0x0F)) > 0x0F);
}
bool check_h_sub_8bit(uint8_t a, uint8_t b)
{
    return ((a & 0x0F) < (b & 0x0F));
}
bool check_h_add_16bit(uint16_t a, uint16_t b)
{
    return ((a & 0x0FFF) + (b & 0x0FFF)) > 0x0FFF;
}
bool check_c_add_16bit(uint16_t a, uint16_t b)
{
    return ((uint32_t)a + (uint32_t)b > 0xFFFF);
}
bool check_c_add_8bit(uint8_t a, uint8_t b)
{
    return ((uint16_t)a + (uint16_t)b) > 0xFF;
}
bool check_c_sub_8bit(uint8_t a, uint16_t b)
{
    return a < b;
}