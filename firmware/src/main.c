#include <stdint.h>
#include <stdbool.h>
#include "printf/printf.h"

#include "util.h"
#include "interrupts.h"
#include "palette.h"
#include "input.h"

#define CLK_MHZ 24
#define CLK_KHZ (CLK_MHZ * 1000)
#define MS_TO_TICKS(ms) ((ms) * CLK_KHZ)

#define RGB(r, g, b) ( ( ((r) & 0xf8) << 7 ) | ( ((g) & 0xf8) << 2 ) | ( ((b) & 0xf8) >> 3 ) );

#define WAIT_CLEAR_TICKS MS_TO_TICKS(100)
#define MIN_SAMPLE_TICKS MS_TO_TICKS(100)
#define MAX_SAMPLE_TICKS MS_TO_TICKS(300)

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
volatile uint16_t *hps = (volatile uint16_t *)0x500000;
volatile uint16_t *hps_valid = (volatile uint16_t *)0x510000;

volatile uint32_t int2_count = 0;

volatile uint16_t sample_seq = 0; 
volatile uint16_t frame_seq = 0;
volatile uint16_t sensor_seq = 0;

volatile uint32_t frame_ticks = 0;
volatile uint32_t sensor_ticks = 0;

typedef struct
{
    uint16_t size;

    int16_t vsync_adjust;
    char modeline[64];
} HPSConfig;

HPSConfig hps_config;

void commit_hps_config()
{
    *hps_valid = 0;
    memcpy((void *)hps, &hps_config, sizeof(hps_config));
    *hps_valid = 0xffff;
}

void init_hps_config()
{
    hps_config.size = sizeof(hps_config);
    hps_config.vsync_adjust = -1;
    hps_config.modeline[0] = '\0';

    commit_hps_config();
}

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
    uint16_t tile = ( ( color & 0xf0 ) << 8 ) | ( 0x10 + ( color & 0x0f ) );

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

#define STATUS_W 24
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
    draw_rect(0x8f, 0, 1, 11, 6);
    draw_rect(0x8f, 0, 11, 11, 6);
    draw_rect(0x8f, 0, 21, 11, 6);

    for( int i = 0; i < 16; i++ )
    {
        draw_rect(0x90 + i, 16+i, 21, 1, 10);
    }

    status.x = 14;
    status.y = 10;
}


typedef enum
{
    ST_CLEAR = 0,
    ST_WAIT_CLEAR,
    ST_START_SAMPLE,
    ST_WAIT_SAMPLE,
    ST_END_SAMPLE,
} State;

State state = ST_CLEAR;
uint32_t state_start_ticks;

static void set_state(State s)
{
    state = s;
    state_start_ticks = read_ticks();
}

int ticks_to_ms_str(uint32_t ticks, char *str, int len)
{
    uint32_t ms = ticks / CLK_KHZ;
    uint32_t us = (ticks / CLK_MHZ ) % 1000;
    return snprintf(str, len, "%u.%u", ms, us);
}

#define HISTORY_SIZE 32
uint32_t samples[HISTORY_SIZE];
uint32_t latest_sample;
uint32_t sample_idx = 0;
typedef enum
{
    NO_SAMPLE,
    NEW_SAMPLE,
    MISSING_SAMPLE
} SampleStatus;

SampleStatus sample_status;

void record_new_sample(uint32_t ticks)
{
    latest_sample = ticks;
    samples[sample_idx % HISTORY_SIZE] = ticks;
    sample_idx++;
    sample_status = NEW_SAMPLE;
}

void record_missing_sample()
{
    sample_status = MISSING_SAMPLE;
}

void update_sample_status()
{
    uint32_t min_ticks = 0xffffffff;
    uint32_t max_ticks = 0x00000000;
    uint32_t total_ticks = 0;

    int history_len = HISTORY_SIZE > sample_idx ? sample_idx : HISTORY_SIZE;

    for( int i = 0; i < history_len; i++ )
    {
        uint32_t ticks = samples[i];
        total_ticks += ticks;

        if (ticks > max_ticks) max_ticks = ticks;
        if (ticks < min_ticks) min_ticks = ticks;
    }

    uint32_t mean_ticks = total_ticks / history_len;

    status.colors[0] = 0;
    status.colors[1] = 0;
    status.colors[2] = 0;
    status.colors[3] = 0;

    char ms_str1[16], ms_str2[16];
    ticks_to_ms_str(latest_sample, ms_str1, 16);
    ticks_to_ms_str(mean_ticks, ms_str2, 16);
    snprintf(status.lines[0], STATUS_W, "Cur: %s ms", ms_str1);
    snprintf(status.lines[1], STATUS_W, "Avg: %s ms", ms_str2);

    ticks_to_ms_str(min_ticks, ms_str1, 16);
    ticks_to_ms_str(max_ticks, ms_str2, 16);
    snprintf(status.lines[2], STATUS_W, "Min: %s ms", ms_str1);
    snprintf(status.lines[3], STATUS_W, "Max: %s ms", ms_str2);
}

int main(int argc, char *argv[])
{
    char tmp[64];

    init_hps_config();

    memsetw(vram, 0, 128 * 128);
    memcpyw(palette_ram, game_palette, 256);
    palette_ram[0x8f] = 0x0000;

    for( int i = 0; i < 16; i++ )
    {
        palette_ram[0x90 + i] = RGB( i * 16, i * 16, i * 16 );
    }

    *user_io = 0xffff;


    memset(&status, 0, sizeof(status));
    config_ntsc_224p();

    enable_interrupts();

    while (true)
    {
        wait_vblank();
        input_poll();

        uint32_t cur_ticks = read_ticks();
        uint32_t state_ticks = cur_ticks - state_start_ticks;

        switch (state)
        {
            case ST_CLEAR:
                palette_ram[0x8f] = 0x0000;
                set_state(ST_WAIT_CLEAR);
                break;

            case ST_WAIT_CLEAR:
                if (state_ticks >= WAIT_CLEAR_TICKS)
                {
                    set_state(ST_START_SAMPLE);
                }
                break;

            case ST_START_SAMPLE:
                set_state(ST_WAIT_SAMPLE);
                palette_ram[0x8f] = 0xffff;
                sample_seq++;
                break;

            case ST_WAIT_SAMPLE:
                if (state_ticks < MIN_SAMPLE_TICKS)
                {
                    break;
                }

                if (state_ticks > MAX_SAMPLE_TICKS)
                {
                    set_state(ST_CLEAR);
                    record_missing_sample();
                }
                else if (sample_seq == frame_seq && sample_seq == sensor_seq)
                {
                    set_state(ST_CLEAR);
                    uint32_t tick_diff = sensor_ticks - frame_ticks;
                    record_new_sample(tick_diff);
                }
                break;

            default:
                set_state(ST_CLEAR);
                break;
        }

        draw_status();

        switch (sample_status)
        {
            case MISSING_SAMPLE:
                status.colors[0] = 1;
                snprintf(status.lines[0], STATUS_W, "CUR: NO SAMPLE");
                break;

            case NEW_SAMPLE:
                update_sample_status();
                break;

            case NO_SAMPLE:
            default:
                break;
        }

        //snprintf(status.lines[3], STATUS_W, "USER: %02X", *user_io);

        sample_status = NO_SAMPLE;
    }

    return 0;
}

