#include <stdint.h>
#include <stdbool.h>
#include "printf/printf.h"

#include "util.h"
#include "interrupts.h"
#include "palette.h"
#include "input.h"
#include "hdmi.h"
#include "gfx.h"

#define CLK_MHZ 24
#define CLK_KHZ (CLK_MHZ * 1000)
#define MS_TO_TICKS(ms) ((ms) * CLK_KHZ)

#define RGB(r, g, b) ( ( ((r) & 0xf8) << 7 ) | ( ((g) & 0xf8) << 2 ) | ( ((b) & 0xf8) >> 3 ) );

#define WAIT_CLEAR_TICKS MS_TO_TICKS(100)
#define MIN_SAMPLE_TICKS MS_TO_TICKS(200)
#define MAX_SAMPLE_TICKS MS_TO_TICKS(500)

typedef volatile struct
{
    uint32_t value;
    uint16_t latch;
} Ticks;

uint16_t *palette_ram = (uint16_t *)0x920000;
Ticks *ticks = (Ticks *)0x200000;
volatile uint16_t *user_io = (volatile uint16_t *)0x300000;

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
    gfx_begin_region(status.x, status.y, STATUS_W, STATUS_H);
    gfx_pen256(0);
    gfx_rect(0, 0, STATUS_W, STATUS_H);

    for( int i = 0; i < STATUS_H; i++ )
    {
        gfx_pen16(status.colors[i]);
        gfx_text(status.lines[i]);
    }
    gfx_end_region();
}

#define NUM_HDMI_MODES 15

HDMIMode hdmi_modes[NUM_HDMI_MODES] =
{
    {    0,   0,   0,   0,    0,  0,  0,  0,   0    }, //0  Default
    { 1280, 110,  40, 220,  720,  5,  5, 20,  74.25 }, //1  1280x720@60
	{ 1024,  24, 136, 160,  768,  3,  6, 29,  65,   }, //2  1024x768@60
	{  720,  16,  62,  60,  480,  9,  6, 30,  27    }, //3  720x480@60
	{  720,  12,  64,  68,  576,  5,  5, 39,  27    }, //4  720x576@50
	{ 1280,  48, 112, 248, 1024,  1,  3, 38, 108    }, //5  1280x1024@60
	{  800,  40, 128,  88,  600,  1,  4, 23,  40    }, //6  800x600@60
	{  640,  16,  96,  48,  480, 10,  2, 33,  25.175 }, //7  640x480@60
	{ 1280, 440,  40, 220,  720,  5,  5, 20,  74.25  }, //8  1280x720@50
	{ 1920,  88,  44, 148, 1080,  4,  5, 36, 148.5   }, //9  1920x1080@60
	{ 1920, 528,  44, 148, 1080,  4,  5, 36, 148.5   }, //10  1920x1080@50
	{ 1366,  70, 143, 213,  768,  3,  3, 24,  85.5   }, //11 1366x768@60
	{ 1024,  40, 104, 144,  600,  1,  3, 18,  48.96  }, //12 1024x600@60
	{ 1920,  48,  32,  80, 1440,  2,  4, 38, 185.203 }, //13 1920x1440@60
	{ 2048,  48,  32,  80, 1536,  2,  4, 38, 209.318 }, //14 2048x1536@60
};

const char *hdmi_mode_names[NUM_HDMI_MODES] =
{
    "Default",
    "1280x720 60Hz",
    "1024x768 60Hz",
    "720x480 60Hz",
    "720x576 50Hz",
    "1280x1024 60Hz",
    "800x600 60Hz",
    "640x480 60Hz",
    "1280x720 50Hz",
    "1920x1080 60Hz",
    "1920x1080 50Hz",
    "1366x768 60Hz",
    "1024x600 60Hz",
    "1920x1440 60Hz",
    "2048x1536 60Hz"
};

const char *vsync_modes[3] = { "Match Output", "Match Input", "Low Latency" };
static bool draw_menu(bool reset)
{
    static int mode_idx = 0;
    static int vsync_adjust = 0;

    static int applied_mode_idx = 0;
    static int applied_vsync_adjust = 0;

    static MenuContext menuctx = INIT_MENU_CONTEXT;

    if (reset)
    {
        mode_idx = applied_mode_idx;
        vsync_adjust = applied_vsync_adjust;
    }

    gfx_clear();

    gfx_begin_menu("VIDEO CONFIG", 20, 20, &menuctx);
    
    gfx_menuitem_select("HDMI Resolution", hdmi_mode_names, NUM_HDMI_MODES, &mode_idx);
    if( mode_idx != 0 )
        gfx_menuitem_select("VSync Mode", vsync_modes, 3, &vsync_adjust);

    if (mode_idx != applied_mode_idx || vsync_adjust != applied_vsync_adjust)
    {
        if (gfx_menuitem_button("Apply Changes"))
        {
            if( mode_idx )
                hdmi_set_mode(&hdmi_modes[mode_idx], vsync_adjust, 60.0);
            else
                hdmi_clear_mode();
            applied_mode_idx = mode_idx;
            applied_vsync_adjust = vsync_adjust;
        }
    }

    gfx_end_menu();

    if (input_pressed() & (INPUT_MENU | INPUT_BACK))
    {
        return false;
    }
    else
    {
        return true;
    }
}

uint32_t vblank_count = 0;
void wait_vblank()
{
    while (vblank_count == int2_count) {};

    vblank_count = int2_count;
}


static void init_sampling_ui()
{
    for( int i = 0; i < 2; i++ )
    {
        gfx_pageflip();
        gfx_clear();
        gfx_pen256(0x8f);
        gfx_rect(0, 1, 11, 6);
        gfx_rect(0, 11, 11, 6);
        gfx_rect(0, 21, 11, 6);
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

typedef enum { MODE_SAMPLING, MODE_MENU } MainMode;

void do_sampling()
{
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

    snprintf(status.lines[3], STATUS_W, "GAMEPAD: %02X", input_state());

    sample_status = NO_SAMPLE;
}

int main(int argc, char *argv[])
{
    char tmp[64];

    MainMode mode = MODE_SAMPLING;

    memcpyw(palette_ram, game_palette, 256);
    palette_ram[0x8f] = 0x0000;

    for( int i = 0; i < 16; i++ )
    {
        palette_ram[0x90 + i] = RGB( i * 16, i * 16, i * 16 );
    }

    *user_io = 0xffff;

    memset(&status, 0, sizeof(status));
    gfx_set_ntsc_224p();

    enable_interrupts();

    bool new_mode = true;

    while (true)
    {
        wait_vblank();
        gfx_pageflip();
        input_poll();

        if (mode == MODE_SAMPLING)
        {
            if (new_mode)
            {
                init_sampling_ui();
                new_mode = false;
            }
            else
            {
                do_sampling();
                if (input_pressed() & INPUT_MENU)
                {
                    mode = MODE_MENU;
                    new_mode = true;
                }
            }
        }
        else if (mode == MODE_MENU)
        {
            if( !draw_menu(new_mode) )
            {
                mode = MODE_SAMPLING;
                new_mode = true;
            }
            else
            {
                new_mode = false;
            }
        }

    }

    return 0;
}

