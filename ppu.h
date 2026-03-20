#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include <stdbool.h>

#define HEIGHT 144
#define WIDTH 160
#define SCALE 4

// PPU registers
#define LCDC 0xFF40
#define STAT 0xFF41
#define SCY 0xFF42
#define SCX 0xFF43
#define LY 0xFF44
#define LYC 0xFF45
#define BGP 0xFF47
#define OBP0 0xFF48
#define OBP1 0xFF49
#define WY 0xFF4A
#define WX 0xFF4B

extern uint32_t pixel_buffer[WIDTH * HEIGHT];
extern const uint32_t colors[4];

void update_ppu(int cycles);
void render_scanline(void);
void render_sprites(void);

#endif
