#include <stdint.h>
#include <stdio.h>

struct registers
{
    uint8_t A; // Accumulator
    uint16_t BC;
    uint16_t DE;
    uint16_t HL;
    uint16_t SP; // Stack Pointer
    uint16_t PC; // Program Counter
};
uint8_t memory[0x10000];

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Incorrect number of rom names provided \n Usage: $%s {rom name}\n", argv[0]);
        return 0;
    };
    FILE *rom = fopen(argv[1], "rb");
    if (rom == NULL)
    {
        printf("An error occurred when trying to open the ROM\n");
        return 0;
    }

    size_t bytes_read = fread(memory, 1, 0x8000, rom);
}