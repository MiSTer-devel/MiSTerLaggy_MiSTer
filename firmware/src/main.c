#include <stdint.h>
#include <stdbool.h>
#include "printf/printf.h"

#include "util.h"
#include "interrupts.h"
#include "palette.h"

typedef volatile struct
{
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
uint16_t *vram = (uint16_t *)0x900000;
uint16_t *palette_ram = (uint16_t *)0x910000;
Ticks *ticks = (Ticks *)0x200000;

uint32_t int2_count = 0;
uint32_t int4_count = 0;
uint32_t int6_count = 0;

__attribute__((interrupt)) void level2_handler()
{
    int2_count++;
}

__attribute__((interrupt)) void level4_handler()
{
    int4_count++;
}

__attribute__((interrupt)) void level6_handler()
{
    int6_count++;
}

uint32_t read_ticks()
{
    ticks->latch = 0xffff;
    return ticks->value;
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


volatile uint32_t vblank_count = 0;

int main(int argc, char *argv[])
{
    char tmp[64];
    memcpyw(palette_ram, game_palette, 256);

    crtc->h_start = 16;
    crtc->h_end = 500;
    crtc->hact_end = 335;
    crtc->hsync_start = 400;
    crtc->hsync_end = 440;

    crtc->v_start = 0;
    crtc->v_end = 700;
    crtc->vact_end = 239;
    crtc->vsync_start = 270;
    crtc->vsync_end = 280;

    enable_interrupts();

    while (true)
    {
        snprintf(tmp, 64, "TICKS: %010u", read_ticks());
        draw_text(0, 10, 10, tmp);

        snprintf(tmp, 64, "INT2:  %010u", int2_count);
        draw_text(0, 10, 12, tmp);

        snprintf(tmp, 64, "INT4:  %010u", int4_count);
        draw_text(0, 10, 14, tmp);

        snprintf(tmp, 64, "INT6:  %010u", int6_count);
        draw_text(0, 10, 16, tmp);
    }

    return 0;
}

