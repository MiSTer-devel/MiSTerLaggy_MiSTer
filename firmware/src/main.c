#include <stdint.h>
#include <stdbool.h>
#include "printf/printf.h"

#include "util.h"
#include "interrupts.h"
#include "input.h"
#include "hdmi.h"
#include "gfx.h"
#include "clock.h"
#include "debug.h"

#define FIRMWARE_VERSION "1.3"

#define RGB(r, g, b) ( ( ((r) & 0xf8) << 7 ) | ( ((g) & 0xf8) << 2 ) | ( ((b) & 0xf8) >> 3 ) )

#define WAIT_CLEAR_TICKS CLOCK_MS_TO_TICKS(200)
#define MIN_SAMPLE_TICKS CLOCK_MS_TO_TICKS(200)
#define MAX_SAMPLE_TICKS CLOCK_MS_TO_TICKS(500)

uint16_t *palette_ram = (uint16_t *)0x920000;
volatile uint16_t *user_io = (volatile uint16_t *)0x300000;
uint16_t *int_ctrl = (uint16_t *)0x700000;
uint32_t *core_version = (uint32_t *)0xf00000;

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


__attribute__((interrupt)) void level2_handler()
{
    DEBUG_FRAME_MARKER(vblank_start);
    vblank_int_count++;
}

__attribute__((interrupt)) void level4_handler()
{
    if (frame_seq != sample_seq)
    {
        frame_ticks = clock_get_ticks();
        frame_seq = sample_seq;
    }

    DEBUG_FRAME_MARKER(vblank_end);
}

__attribute__((interrupt)) void level6_handler()
{
    if (sensor_seq != sample_seq)
    {
        sensor_ticks = clock_get_ticks();
        sensor_seq = sample_seq;
    }
}


static uint32_t rand_state = 0xdeadbeef;
uint32_t rand32()
{
 	uint32_t x = rand_state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
    rand_state = x;
	return x;
}


#define STATUS_W 16
#define STATUS_H 4
typedef struct
{
    char lines[STATUS_H][STATUS_W+1];
    uint8_t colors[STATUS_H];
} StatusInfo;

StatusInfo status;

int test_position = 0;
static inline Align align_test()
{
    return test_position == 0 ? ALIGN_LEFT : ALIGN_RIGHT;
}

static inline Align align_info()
{
    return test_position == 0 ? ALIGN_RIGHT : ALIGN_LEFT;
}

char video_mode_desc[32];

static void draw_status()
{
    gfx_begin_window(ALIGN_MIDDLE | align_info(), 2, -2, 24, STATUS_H, 0);

    for( int i = 0; i < STATUS_H; i++ )
    {
        gfx_pen(status.colors[i]);
        gfx_text(status.lines[i]);
    }

    gfx_end_window();
}

typedef struct
{
    uint16_t width;
    uint16_t height;
    bool wide;
} HDMIResolution;

static const HDMIResolution hdmi_resolutions[] =
{
    {  640,  480, false },
    {  800,  600, false },
    { 1280,  720, true },
    { 1024,  768, false },
    { 1366,  768, false },
    { 1280,  960, false },
    { 1708,  960, true },
    { 1920, 1080, true },
    { 1600, 1200, false },
    { 1792, 1344, false },
    { 1920, 1440, false },
    { 2048, 1536, false }
};

static const int hdmi_refresh_rates[] =
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
    60,
    61,
    62,
    63,
    64,
    65,
    72
};

static const char *resolution_to_string(const void *options, int index)
{
    static char tmp[32];

    const HDMIResolution *res = (const HDMIResolution *)options;

    snprintf(tmp, sizeof(tmp), "%ux%u", res[index].width, res[index].height);

    return tmp;
}

static const char *refresh_to_string(const void *options, int index)
{
    static char tmp[10];

    const int *hz = (const int *)options;

    snprintf(tmp, sizeof(tmp), "%dhz", hz[index]);

    return tmp;
}

static bool draw_menu(bool reset)
{
    static int mode_idx = 0;
    static int refresh_idx = 0;
    static int aspect_idx = 0;
    static int applied_mode_idx = -1;
    static int applied_refresh_idx = -1;
    static int applied_aspect_idx = -1;

    static MenuContext menuctx = INIT_MENU_CONTEXT;

    bool close_menu = false;

    if (reset)
    {
        if (applied_mode_idx >= 0)
        {
            mode_idx = applied_mode_idx;
            refresh_idx = applied_refresh_idx;
            aspect_idx = applied_aspect_idx;
        }
        else
        {
            mode_idx = 7;
            refresh_idx = 12;
            aspect_idx = 0;
        }
    }

    gfx_clear();

    gfx_begin_menu("CONFIG", 28, 18, &menuctx);
    
    gfx_menuitem_select_func("Resolution", hdmi_resolutions, ARRAY_COUNT(hdmi_resolutions), resolution_to_string, &mode_idx);
    gfx_menuitem_select_func("Refresh Rate", hdmi_refresh_rates, ARRAY_COUNT(hdmi_refresh_rates), refresh_to_string, &refresh_idx);

    if (hdmi_resolutions[mode_idx].wide)
    {
        const char *aspects[2] = { "4:3", "16:9" };
        gfx_menuitem_select("Aspect Ratio", aspects, 2, &aspect_idx);
    }
    else
    {
        gfx_newline(2);
    }

    gfx_newline(1);

    if (mode_idx != applied_mode_idx || refresh_idx != applied_refresh_idx || aspect_idx != applied_aspect_idx)
    {
        if (gfx_menuitem_button("Apply Video Changes"))
        {
            bool wide = hdmi_resolutions[mode_idx].wide && (aspect_idx == 1);
            *int_ctrl = INT2_CTRL(INT_SRC_VBLANK) | INT4_CTRL(INT_SRC_HDMI_VBLANK | INT_INVERT) | INT6_CTRL(INT_SRC_USERIO);
            gfx_set_240p(hdmi_refresh_rates[refresh_idx], wide);
            hdmi_set_mode(hdmi_resolutions[mode_idx].width, hdmi_resolutions[mode_idx].height, hdmi_refresh_rates[refresh_idx]);
            applied_mode_idx = mode_idx;
            applied_refresh_idx = refresh_idx;
            applied_aspect_idx = aspect_idx;
            close_menu = true;

            snprintf(video_mode_desc, sizeof(video_mode_desc), "%s @ %s",
                        resolution_to_string(hdmi_resolutions, mode_idx),
                        refresh_to_string(hdmi_refresh_rates, refresh_idx));
        }
    }

    gfx_newline(2);
    const char *test_positions[2] = { "Left", "Right" };
    gfx_menuitem_select("Test Position", test_positions, 2, &test_position);

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
    return snprintf(str, len, "%u.%03u", ms, us);
}

#define HISTORY_SIZE 16
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

typedef enum { MODE_NO_SENSOR, MODE_SAMPLING, MODE_MENU } MainMode;

#define BAR_W 11
#define BAR_H 4

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
            DEBUG_FRAME_MARKER(sample_start);
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
                if (frame_ticks >= sensor_ticks)
                {
                    record_missing_sample();
                }
                else
                {
                    uint32_t tick_diff = sensor_ticks - frame_ticks;
                    record_new_sample(tick_diff);
                }
            }
            break;

        default:
            set_state(ST_CLEAR);
            break;
    }


    gfx_clear();
    gfx_pen(TEXT_DARK_GRAY);
    gfx_display_border();

    gfx_pen(0x80);

    int16_t rx, ry;
    gfx_align_box(align_test() | ALIGN_TOP, 0, 0, BAR_W, BAR_H, &rx, &ry);
    gfx_rect(rx, ry, BAR_W, BAR_H);

    gfx_align_box(align_test() | ALIGN_MIDDLE, 0, 0, BAR_W, BAR_H, &rx, &ry);
    gfx_rect(rx, ry, BAR_W, BAR_H);

    gfx_align_box(align_test() | ALIGN_BOTTOM, 0, 0, BAR_W, BAR_H, &rx, &ry);
    gfx_rect(rx, ry, BAR_W, BAR_H);

    gfx_align_box(align_info() | ALIGN_TOP, 2, 2, 4, 4, &rx, &ry);
    if ((rand32() & 0xff33) == 0x0000)
        gfx_image(0x80 + 16, 0x40, rx, ry, 4, 4);
    else
        gfx_image(0x80, 0x40, rx, ry, 4, 4);

    gfx_begin_window(ALIGN_BOTTOM | align_info(), 2, 5, 24, 2, 0);
    gfx_pen(TEXT_BLUE);
    gfx_textf("Mode: %s", video_mode_desc);
    gfx_pen(TEXT_DARK_BLUE);
    gfx_text("Press START for menu.");
    gfx_end_window();

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

void draw_no_sensor()
{
    gfx_clear();
    gfx_pen(TEXT_RED);
    gfx_begin_window(ALIGN_CENTER | ALIGN_MIDDLE, 0, 0, 34, 6, 1);

    gfx_text_aligned(ALIGN_CENTER, "No sensor detected!");
    gfx_newline(1);
    gfx_pen(TEXT_ORANGE);
    gfx_text_aligned(ALIGN_LEFT, "Connect your MiSTer Laggy sensor");
    gfx_text_aligned(ALIGN_LEFT, "to the User Port.");
    
    gfx_end_window();
}

void draw_version()
{
    gfx_pen(TEXT_DARK);
    gfx_begin_window(ALIGN_BOTTOM | align_info(), 2, 2, 24, 1, 0);
    gfx_textf_aligned(ALIGN_LEFT, "MiSTer Laggy %s/%06u", FIRMWARE_VERSION, *core_version);
    gfx_end_window();
}

uint16_t base_palette[16] =
{
    RGB(0x3f, 0x28, 0x32),
    RGB(0x68, 0x60, 0x5c),
    RGB(0xb0, 0xb0, 0xb8),
    RGB(0xfc, 0xfc, 0xfc),
    RGB(0x1c, 0x38, 0xac),
    RGB(0x70, 0x70, 0xfc),
    RGB(0xa8, 0x28, 0x14),
    RGB(0xfc, 0x48, 0x48),
    RGB(0x20, 0x88, 0x00),
    RGB(0x70, 0xf8, 0x28),
    RGB(0xb8, 0x2c, 0xd0),
    RGB(0xfc, 0x74, 0xec),
    RGB(0xac, 0x58, 0x1c),
    RGB(0xf8, 0xa8, 0x50),
    RGB(0x3c, 0xd4, 0xe4),
    RGB(0xf8, 0xec, 0x20)
};

void set_palette()
{
    for( int i = 0; i < 16; i++ )
    {
        palette_ram[(i << 1) + 0] = RGB(0, 0, 0);
        palette_ram[(i << 1) + 1] = base_palette[i];
    }

    palette_ram[0x20] = RGB(0,0,0);
    palette_ram[0x21] = RGB(0xff,0xff,0xff);

    /* Misterkun */
    palette_ram[0x40] = RGB(0,0,0);
    palette_ram[0x41] = RGB(8,8,8);
    palette_ram[0x42] = RGB(187,187,187);
    palette_ram[0x43] = RGB(232,126,182);
    palette_ram[0x44] = RGB(240,148,191);
    palette_ram[0x45] = RGB(247,246,246);
    palette_ram[0x46] = RGB(255,255,255);
    palette_ram[0x47] = RGB(52,52,72);

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

    gfx_set_240p(60, false);

    enable_interrupts();

    bool new_mode = true;

    while (true)
    {
        wait_vblank();
        gfx_pageflip();
        input_poll();

        if (mode == MODE_SAMPLING)
        {
            new_mode = false;
            do_sampling();
            if (input_pressed() & INPUT_MENU)
            {
                mode = MODE_MENU;
                new_mode = true;
            }

            if (*user_io & 0x0001)
            {
                mode = MODE_NO_SENSOR;
                new_mode = true;
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
        else if (mode == MODE_NO_SENSOR)
        {
            if (!(*user_io & 0x0001))
            {
                mode = MODE_SAMPLING;
                new_mode = true;
            }
            draw_no_sensor();
        }

        draw_version();

        DEBUG_DRAW();

        DEBUG_FRAME_MARKER(frontend_end);
        DEBUG_END_FRAME();
    }

    return 0;
}

