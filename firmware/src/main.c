#include <stdint.h>
#include <stdbool.h>
#include "printf/printf.h"

#include "util.h"
#include "interrupts.h"
#include "palette.h"

typedef volatile struct
{
    uint16_t clk_numer;
    uint16_t clk_denom;

    uint16_t h_start;
    uint16_t h_end;
    uint16_t hact_end;
    uint16_t hsync_start;
    uint16_t hsync_end;
    uint16_t v_start;
    uint16_t v_end;
    uint16_t vact_end;
    uint16_t vsync_start;
    uint16_t vsync_end;

    uint16_t hcnt;
    uint16_t vcnt;
} CRTC;

typedef volatile struct
{
    uint32_t value;
    uint16_t latch;
} Ticks;


CRTC *crtc = (CRTC *)0x800000;
uint16_t *vram = (uint16_t *)0x900000; // 128 * 128 = 16384 words
uint16_t *palette_ram = (uint16_t *)0x910000;
Ticks *ticks = (Ticks *)0x200000;
volatile uint16_t *user_io = (volatile uint16_t *)0x300000;
volatile uint16_t *gamepad = (volatile uint16_t *)0x400000;

volatile uint32_t int2_count = 0;

volatile uint16_t sample_seq = 0; 
volatile uint16_t frame_seq = 0;
volatile uint16_t sensor_seq = 0;

volatile uint32_t frame_ticks = 0;
volatile uint32_t sensor_ticks = 0;

uint32_t read_ticks()
{
    disable_interrupts();
    ticks->latch = 0xffff;
    uint32_t res = ticks->value;
    enable_interrupts();

    return res;
}

__attribute__((interrupt)) void level2_handler()
{
    int2_count++;
}

__attribute__((interrupt)) void level4_handler()
{
    if (frame_seq != sample_seq)
    {
        frame_ticks = read_ticks();
        frame_seq = sample_seq;
    }
}

__attribute__((interrupt)) void level6_handler()
{
    if (sensor_seq != sample_seq)
    {
        sensor_ticks = read_ticks();
        sensor_seq = sample_seq;
    }
}


void draw_rect(int color, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    int ofs = ( y * 128 ) + x;
    uint16_t tile = (color << 12) | 0x01;

    for( int i = 0; i < h; i++ )
    {
        memsetw(&vram[ofs], tile, w);
        ofs += 128;
    }
}


void draw_text(int color, uint16_t x, uint16_t y, const char *str)
{
    int ofs = ( y * 128 ) + x;

    while(*str)
    {
        if( *str == '\n' )
        {
            y++;
            ofs = (y * 128) + x;
        }
        else
        {
            vram[ofs] = (color << 12) | *str;
            ofs++;
        }
        str++;
    }
}


uint32_t vblank_count = 0;

void wait_vblank()
{
    while (vblank_count == int2_count) {};

    vblank_count = int2_count;
}

int main(int argc, char *argv[])
{
    char tmp[64];
    memcpyw(palette_ram, game_palette, 256);

    crtc->clk_numer = 1;
    crtc->clk_denom = 3;

    crtc->h_start = 16;
    crtc->h_end = 637;
    crtc->hact_end = 529;
    crtc->hsync_start = 544;
    crtc->hsync_end = 590;

    crtc->v_start = 0;
    crtc->v_end = 253;
    crtc->vact_end = 239;
    crtc->vsync_start = 243;
    crtc->vsync_end = 247;

    *user_io = 0xffff;

    memsetw(&palette_ram[0x80], 0xffff, 16);

    palette_ram[0x89] = 0x0000;

    draw_rect(8, 4, 2, 6, 6);
    draw_rect(8, 4, 10, 6, 6);
    draw_rect(8, 4, 18, 6, 6);

    enable_interrupts();

    uint32_t recent_us = 0, recent_ms = 0;

    while (true)
    {
        wait_vblank();

        if (vblank_count & 0x20)
        {
            palette_ram[0x89] = 0xffff;
            if ((vblank_count & 0x1f) == 0)
            {
                sample_seq++;
            }
        }
        else
        {
            palette_ram[0x89] = 0x0000;
            if (sensor_ticks >= frame_ticks)
            {
                recent_ms = (sensor_ticks - frame_ticks) / 20000;
                recent_us = ((sensor_ticks - frame_ticks) / 20 ) % 1000;
            }

        }

        snprintf(tmp, 64, "TICKS: %010u", read_ticks());
        draw_text(0, 18, 10, tmp);

        snprintf(tmp, 64, "INT2:  %010u", int2_count);
        draw_text(0, 18, 12, tmp);

        snprintf(tmp, 64, "USER_IO:  %02X", *user_io);
        draw_text(0, 18, 14, tmp);

        snprintf(tmp, 64, "SENSOR:  %u.%u ms          ", recent_ms, recent_us);
        draw_text(0, 18, 16, tmp);
    }

    return 0;
}

