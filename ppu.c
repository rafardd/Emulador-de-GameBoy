#include "ppu.h"
#include "cpu.h"

static int ppu_cycles = 0;
uint32_t pixel_buffer[WIDTH * HEIGHT];
static uint8_t bg_priority[WIDTH];

const uint32_t colors[4] = {
    0xFFFFFFFF, // White (00)
    0xFFC0C0C0, // Light grey (01)
    0xFF606060, // Dark grey (10)
    0xFF000000  // Black (11)
};

void render_sprites(void)
{
    uint8_t lcdc = ram[LCDC];
    if (!(lcdc & 0x02))
        return; // Sprites are off

    bool sprite_8x16 = (lcdc & 0x04);
    uint8_t ly = ram[LY];
    int sprites_on_line = 0;

    for (int i = 0; i < 40 && sprites_on_line < 10; i++)
    {
        uint16_t sprite_addr = 0xFE00 + (i * 4);
        int y_pos = ram[sprite_addr] - 16;
        int x_pos = ram[sprite_addr + 1] - 8;
        uint8_t tile_index = ram[sprite_addr + 2];
        uint8_t attributes = ram[sprite_addr + 3];

        int height = sprite_8x16 ? 16 : 8;

        if (ly >= y_pos && ly < (y_pos + height))
        {
            sprites_on_line++;

            uint8_t palette = (attributes & 0x10) ? ram[OBP1] : ram[OBP0];
            bool flip_x = (attributes & 0x20);
            bool flip_y = (attributes & 0x40);
            bool priority = (attributes & 0x80);

            int line = ly - y_pos;
            if (flip_y)
                line = height - 1 - line;

            if (sprite_8x16)
            {
                tile_index &= 0xFE;
            }

            uint16_t tile_data_ptr = 0x8000 + (tile_index * 16) + (line * 2);
            uint8_t data1 = ram[tile_data_ptr];
            uint8_t data2 = ram[tile_data_ptr + 1];

            for (int tile_x = 0; tile_x < 8; tile_x++)
            {
                int pixel_x = x_pos + tile_x;
                if (pixel_x < 0 || pixel_x >= WIDTH)
                    continue;

                int bit = flip_x ? tile_x : (7 - tile_x);
                uint8_t color_idx = ((data2 >> bit) & 0x01) << 1;
                color_idx |= ((data1 >> bit) & 0x01);

                if (color_idx == 0)
                    continue; // Transparent

                if (priority && bg_priority[pixel_x])
                    continue;

                uint8_t final_color = (palette >> (color_idx * 2)) & 0x03;
                pixel_buffer[ly * WIDTH + pixel_x] = colors[final_color];
            }
        }
    }
}

void render_scanline(void)
{
    uint8_t lcdc = ram[LCDC];
    if (!(lcdc & 0x80))
        return;

    uint8_t ly = ram[LY];
    uint8_t scroll_y = ram[SCY];
    uint8_t scroll_x = ram[SCX];
    uint8_t win_y = ram[WY];
    uint8_t win_x = ram[WX] - 7;

    bool window_enable = (lcdc & 0x20) && (ly >= win_y);
    bool bg_enable = (lcdc & 0x01);

    for (int x = 0; x < WIDTH; x++)
    {
        bg_priority[x] = 0;

        bool is_window = window_enable && (x >= win_x);

        uint16_t tile_map_base;
        uint8_t tx, ty;

        if (is_window)
        {
            tile_map_base = (lcdc & 0x40) ? 0x9C00 : 0x9800;
            tx = x - win_x;
            ty = ly - win_y;
        }
        else if (bg_enable)
        {
            tile_map_base = (lcdc & 0x08) ? 0x9C00 : 0x9800;
            tx = x + scroll_x;
            ty = ly + scroll_y;
        }
        else
        {
            pixel_buffer[ly * WIDTH + x] = colors[0];
            continue;
        }

        uint16_t tile_row = (uint16_t)(ty / 8) * 32;
        uint16_t tile_col = (tx / 8);
        uint16_t tile_address = tile_map_base + tile_row + tile_col;

        int16_t tile_index;
        bool unsigned_addressing = (lcdc & 0x10);
        if (unsigned_addressing)
            tile_index = ram[tile_address];
        else
            tile_index = (int8_t)ram[tile_address];

        uint16_t tile_data_ptr;
        if (unsigned_addressing)
            tile_data_ptr = 0x8000 + (tile_index * 16);
        else
            tile_data_ptr = 0x8800 + ((tile_index + 128) * 16);

        uint8_t line = (ty % 8) * 2;
        uint8_t data1 = ram[tile_data_ptr + line];
        uint8_t data2 = ram[tile_data_ptr + line + 1];

        int bit = 7 - (tx % 8);
        uint8_t color_bit = (((data2 >> bit) & 0x01) << 1) | ((data1 >> bit) & 0x01);

        if (color_bit != 0)
            bg_priority[x] = 1;

        uint8_t palette = ram[BGP];
        uint8_t final_color = (palette >> (color_bit * 2)) & 0x03;

        pixel_buffer[ly * WIDTH + x] = colors[final_color];
    }

    render_sprites();
}

void update_ppu(int cycles)
{
    uint8_t lcdc = ram[LCDC];
    if (!(lcdc & 0x80))
    {
        ram[LY] = 0;
        ppu_cycles = 0;
        ram[STAT] = (ram[STAT] & 0xFC);
        for (int i = 0; i < WIDTH * HEIGHT; i++)
            pixel_buffer[i] = colors[0];
        return;
    }

    ppu_cycles += cycles;

    if (ppu_cycles >= 456)
    {
        ppu_cycles -= 456;

        if (ram[LY] < 144)
        {
            render_scanline();
        }

        ram[LY]++;

        if (ram[LY] == 144)
        {
            ram[0xFF0F] |= 0x01; // Request V-Blank Interrupt
        }
        else if (ram[LY] > 153)
        {
            ram[LY] = 0;
        }

        // Coincidence Flag (LYC == LY)
        if (ram[LY] == ram[LYC])
        {
            ram[STAT] |= 0x04;
            if (ram[STAT] & 0x40)
                ram[0xFF0F] |= 0x02; // STAT Interrupt
        }
        else
        {
            ram[STAT] &= ~0x04;
        }
    }

    // Update STAT and interruptions
    uint8_t mode = 0;
    if (ram[LY] >= 144)
    {
        mode = 1; // V-Blank
    }
    else
    {
        if (ppu_cycles < 80)
            mode = 2; // OAM Search
        else if (ppu_cycles < 252)
            mode = 3; // Pixel Transfer
        else
            mode = 0; // H-Blank
    }

    uint8_t current_mode = ram[STAT] & 0x03;
    if (mode != current_mode)
    {
        ram[STAT] = (ram[STAT] & 0xFC) | mode;
        if (mode == 0 && (ram[STAT] & 0x08))
            ram[0xFF0F] |= 0x02; // H-Blank STAT
        if (mode == 1 && (ram[STAT] & 0x10))
            ram[0xFF0F] |= 0x02; // V-Blank STAT
        if (mode == 2 && (ram[STAT] & 0x20))
            ram[0xFF0F] |= 0x02; // OAM STAT
    }
}
