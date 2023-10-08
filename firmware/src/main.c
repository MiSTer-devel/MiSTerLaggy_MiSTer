#include <stdint.h>
#include <stdbool.h>
#include "printf/printf.h"

#include "util.h"
#include "interrupts.h"
#include "palette.h"
#include "input.h"
#include "hdmi.h"
#include "gfx.h"
#include "clock.h"

#define RGB(r, g, b) ( ( ((r) & 0xf8) << 7 ) | ( ((g) & 0xf8) << 2 ) | ( ((b) & 0xf8) >> 3 ) )

#define WAIT_CLEAR_TICKS CLOCK_MS_TO_TICKS(200)
#define MIN_SAMPLE_TICKS CLOCK_MS_TO_TICKS(200)
#define MAX_SAMPLE_TICKS CLOCK_MS_TO_TICKS(500)

uint16_t *palette_ram = (uint16_t *)0x920000;
volatile uint16_t *user_io = (volatile uint16_t *)0x300000;
uint16_t *int_ctrl = (uint16_t *)0x700000;

#define INT2_CTRL(x) (((x) & 0xf) << 0)
#define INT4_CTRL(x) (((x) & 0xf) << 4)
#define INT6_CTRL(x) (((x) & 0xf) << 8)
#define INT_SRC_NONE 0
#define INT_SRC_VBLANK 1
#define INT_SRC_HDMI_VBLANK 2
#define INT_SRC_USERIO 3
#define INT_INVERT 0x8


volatile uint32_t vblank_int_count = 0;

volatile uint16_t sample_seq = 0; 
volatile uint16_t frame_seq = 0;
volatile uint16_t sensor_seq = 0;

volatile uint32_t frame_ticks = 0;
volatile uint32_t sensor_ticks = 0;

volatile uint32_t prev_vblank_ticks = 0;
volatile uint32_t frame_duration = 0;


__attribute__((interrupt)) void level2_handler()
{
    uint32_t ticks = clock_get_ticks();
    frame_duration = ticks - prev_vblank_ticks;
    prev_vblank_ticks = ticks;

    vblank_int_count++;
}

__attribute__((interrupt)) void level4_handler()
{
    if (frame_seq != sample_seq)
    {
        frame_ticks = clock_get_ticks();
        frame_seq = sample_seq;
    }
}

__attribute__((interrupt)) void level6_handler()
{
    if (sensor_seq != sample_seq)
    {
        sensor_ticks = clock_get_ticks();
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

char video_mode_desc[32];

static void draw_status()
{
    gfx_begin_region(status.x, status.y, STATUS_W, STATUS_H + 1);
    gfx_pen(0);
    gfx_rect(0, 0, STATUS_W, STATUS_H + 1);

    for( int i = 0; i < STATUS_H; i++ )
    {
        gfx_pen(status.colors[i]);
        gfx_text(status.lines[i]);
    }
//    char foo[32];
//    snprintf(foo, sizeof(foo), "%u", frame_duration);
//    gfx_text(foo);

    gfx_end_region();
}

typedef struct
{
    uint16_t width;
    uint16_t height;
} HDMIResolution;

static const HDMIResolution hdmi_resolutions[] =
{
    {  640,  480 },
    {  720,  480 },
    {  720,  576 },
    {  800,  600 },
    { 1024,  600 },
    { 1024,  768 },
    { 1280,  720 },
    { 1280, 1024 },
    { 1366,  768 },
    { 1920, 1080 },
    { 1920, 1440 },
    { 2048, 1536 }
};

static const char *hdmi_resolution_names[] =
{
    "640x480",
    "720x480",
    "720x576",
    "800x600",
    "1024x600",
    "1024x768",
    "1280x720",
    "1280x1024",
    "1366x768",
    "1920x1080",
    "1920x1440",
    "2048x1536"
};

static const float hdmi_refresh_rates[] =
{
    24,
    30,
    50,
    51,
    52,
    53,
    54,
    55,
    56,
    57,
    58,
    59,
    59.94,
    60,
    62,
    72,
    90,
    120,
};

static const char *hdmi_refresh_rate_names[] =
{
    "24hz",
    "30hz",
    "50hz",
    "51hz",
    "52hz",
    "53hz",
    "54hz",
    "55hz",
    "56hz",
    "57hz",
    "58hz",
    "59hz",
    "59.94hz",
    "60hz",
    "62hz",
    "72hz",
    "90hz",
    "120hz"
};


static bool draw_menu(bool reset)
{
    static int mode_idx = 0;
    static int refresh_idx = 0;
    static int applied_mode_idx = -1;
    static int applied_refresh_idx = -1;

    static MenuContext menuctx = INIT_MENU_CONTEXT;

    bool close_menu = false;

    if (reset)
    {
        if (applied_mode_idx >= 0)
        {
            mode_idx = applied_mode_idx;
            refresh_idx = applied_refresh_idx;
        }
        else
        {
            mode_idx = 9;
            refresh_idx = 13;
        }
    }

    gfx_clear();

    gfx_begin_menu("VIDEO CONFIG", 28, 15, &menuctx);
    
    gfx_menuitem_select("Resolution", hdmi_resolution_names, ARRAY_COUNT(hdmi_resolution_names), &mode_idx);
    gfx_menuitem_select("Refresh Rate", hdmi_refresh_rate_names, ARRAY_COUNT(hdmi_refresh_rate_names), &refresh_idx);

    if (mode_idx != applied_mode_idx || refresh_idx != applied_refresh_idx)
    {
        if (gfx_menuitem_button("Apply Changes"))
        {
            *int_ctrl = INT2_CTRL(INT_SRC_VBLANK) | INT4_CTRL(INT_SRC_HDMI_VBLANK | INT_INVERT) | INT6_CTRL(INT_SRC_USERIO);
            gfx_set_240p(hdmi_refresh_rates[refresh_idx]);
            hdmi_set_mode(hdmi_resolutions[mode_idx].width, hdmi_resolutions[mode_idx].height, hdmi_refresh_rates[refresh_idx]);
            applied_mode_idx = mode_idx;
            applied_refresh_idx = refresh_idx;
            close_menu = true;

            snprintf(video_mode_desc, sizeof(video_mode_desc), "%s @ %s", hdmi_resolution_names[mode_idx], hdmi_refresh_rate_names[refresh_idx]);
        }
    }

    gfx_end_menu();

    if (input_pressed() & (INPUT_MENU | INPUT_BACK))
    {
        close_menu = true;
    }

    return !close_menu;
}

uint32_t vblank_count = 0;
void wait_vblank()
{
    while (vblank_count == vblank_int_count) {};

    vblank_count = vblank_int_count;
}


static void init_sampling_ui()
{
    for( int i = 0; i < 2; i++ )
    {
        gfx_pageflip();
        gfx_clear();
        gfx_pen(0x80);
        gfx_rect(0, 0, 11, 4);
        gfx_rect(0, 13, 11, 4);
        gfx_rect(0, 26, 11, 4);

        gfx_begin_region(14, 24, 20, 2);
        gfx_pen(TEXT_AQUA);
        gfx_text("HDMI: "); gfx_sameline(); gfx_text(video_mode_desc);
        gfx_end_region();
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
    state_start_ticks = clock_get_ticks();
}

int ticks_to_ms_str(uint32_t ticks, char *str, int len)
{
    uint32_t ms, us;
    clock_ticks_to_ms_us(ticks, &ms, &us);
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

    status.colors[0] = TEXT_GRAY;
    status.colors[1] = TEXT_GRAY;
    status.colors[2] = TEXT_GRAY;
    status.colors[3] = TEXT_GRAY;

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
    uint32_t cur_ticks = clock_get_ticks();
    uint32_t state_ticks = cur_ticks - state_start_ticks;

    switch (state)
    {
        case ST_CLEAR:
            palette_ram[0x80] = 0x0000;
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
            palette_ram[0x80] = 0xffff;
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
            status.colors[0] = TEXT_ORANGE;
            snprintf(status.lines[0], STATUS_W, "CUR: NO SAMPLE");
            break;

        case NEW_SAMPLE:
            update_sample_status();
            break;

        case NO_SAMPLE:
        default:
            break;
    }

    sample_status = NO_SAMPLE;
}

// https://lospec.com/palette-list/endesga-16
uint16_t base_palette[16] =
{
    RGB(0xe4, 0xa6, 0x72),
    RGB(0xb8, 0x6f, 0x50),
    RGB(0x74, 0x3f, 0x39),
    RGB(0x3f, 0x28, 0x32),
    RGB(0x9e, 0x28, 0x35),
    RGB(0xe5, 0x3b, 0x44),
    RGB(0xfb, 0x92, 0x2b),
    RGB(0xff, 0xe7, 0x62),
    RGB(0x63, 0xc6, 0x4d),
    RGB(0x32, 0x73, 0x45),
    RGB(0x19, 0x3d, 0x3f),
    RGB(0x4f, 0x67, 0x81),
    RGB(0xaf, 0xbf, 0xd2),
    RGB(0xff, 0xff, 0xff),
    RGB(0x2c, 0xe8, 0xf4),
    RGB(0x04, 0x84, 0xd1)
};

void set_palette()
{
    for( int i = 0; i < 16; i++ )
    {
        palette_ram[(i << 1) + 0] = RGB(0, 0, 0);
        palette_ram[(i << 1) + 1] = base_palette[i];
    }
    palette_ram[32] = RGB(0,0,0);
    palette_ram[33] = RGB(0xff,0xff,0xff);
    palette_ram[0x80] = RGB(0,0,0);
}

int main(int argc, char *argv[])
{
    char tmp[64];

    MainMode mode = MODE_SAMPLING;
    strcpy(video_mode_desc, "MiSTer Default");

    set_palette();

    *user_io = 0xffff;
    *int_ctrl = INT2_CTRL(INT_SRC_VBLANK) | INT4_CTRL(INT_SRC_VBLANK | INT_INVERT) | INT6_CTRL(INT_SRC_USERIO);

    memset(&status, 0, sizeof(status));
    gfx_set_240p(60);

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

