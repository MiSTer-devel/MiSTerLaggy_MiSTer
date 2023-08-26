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

    uint16_t hact;
    uint16_t hfp;
    uint16_t hs;
    uint16_t hbp;
    uint16_t vact;
    uint16_t vfp;
    uint16_t vs;
    uint16_t vbp;

    uint16_t hcnt;
    uint16_t vcnt;
} CRTC;

typedef volatile struct
{
    uint32_t value;
    uint16_t latch;
} Ticks;

typedef struct
{
    uint16_t hofs;
    uint16_t vofs;
} TilemapCtrl;


CRTC *crtc = (CRTC *)0x800000;
uint16_t *vram = (uint16_t *)0x900000; // 128 * 128 = 16384 words
TilemapCtrl *tile_ctrl = (TilemapCtrl *)0x910000;

uint16_t *palette_ram = (uint16_t *)0x920000;
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

#define STATUS_W 16
#define STATUS_H 4
typedef struct
{
    uint16_t x;
    uint16_t y;

    char lines[STATUS_H][STATUS_W+1];
    uint8_t colors[STATUS_H];
} StatusInfo;

StatusInfo status;

static void draw_status()
{
    uint16_t ofs = ( status.y * 128 ) + status.x;
    for( int i = 0; i < STATUS_H; i++ )
    {
        uint16_t x = 0;
        const char *s = status.lines[i];
        uint16_t color = status.colors[i] << 12;
        memsetw(&vram[ofs], 0, STATUS_W);

        while(*s && x < STATUS_W)
        {
            vram[ofs + x] = color | *s;
            s++;
            x++;
        }

        ofs += 128;
    }

}

uint32_t vblank_count = 0;
void wait_vblank()
{
    while (vblank_count == int2_count) {};

    vblank_count = int2_count;
}


static void config_ntsc_224p()
{
    crtc->clk_numer = 1;
    crtc->clk_denom = 4;

    crtc->hact = 320;
    crtc->hfp = 8;
    crtc->hs = 32;
    crtc->hbp = 40;

    crtc->vact = 224;
    crtc->vfp = 10;
    crtc->vs = 6;
    crtc->vbp = 10;

    tile_ctrl->hofs = -40;
    tile_ctrl->vofs = -10;

    // Sample rectangles
    draw_rect(8, 0, 1, 11, 6);
    draw_rect(8, 0, 11, 11, 6);
    draw_rect(8, 0, 21, 11, 6);

    status.x = 14;
    status.y = 10;
}

int main(int argc, char *argv[])
{
    char tmp[64];
    memsetw(vram, 0, 128 * 128);
    memcpyw(palette_ram, game_palette, 256);
    palette_ram[0x89] = 0x0000;

    *user_io = 0xffff;

    memset(&status, 0, sizeof(status));
    config_ntsc_224p();

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
                recent_ms = (sensor_ticks - frame_ticks) / 24000;
                recent_us = ((sensor_ticks - frame_ticks) / 24 ) % 1000;
            }
        }
        draw_status();

        status.colors[0] = 0;
        status.colors[1] = 0;
        status.colors[2] = 0;
        status.colors[3] = 0;

        snprintf(status.lines[0], STATUS_W, "TICKS: %u", read_ticks());
        snprintf(status.lines[1], STATUS_W, "INT2: %u", int2_count);
        snprintf(status.lines[2], STATUS_W, "USER_IO: %02X", *user_io);
        snprintf(status.lines[3], STATUS_W, "SENSOR: %u.%u ms", recent_ms, recent_us);
    }

    return 0;
}

