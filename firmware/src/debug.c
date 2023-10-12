#if ML_DEBUG

#include <stddef.h>

#include "debug.h"
#include "util.h"
#include "gfx.h"


DebugStats debug_stats;

static uint32_t vblank_start;
static uint32_t vblank_end;
static uint32_t frame_total;
static uint32_t frontend_end;
static uint32_t sample_start;

void debug_end_frame()
{
    frame_total = debug_stats.vblank_start - vblank_start;
    vblank_start = debug_stats.vblank_start;

    frontend_end = debug_stats.frontend_end - vblank_start;
    vblank_end = debug_stats.vblank_end - vblank_start;

    if (debug_stats.sample_start > debug_stats.vblank_start)
    {
        sample_start = debug_stats.sample_start - debug_stats.vblank_start;
    }
}

void debug_draw()
{
    gfx_begin_window(ALIGN_TOP | ALIGN_RIGHT, 2, 1, 24, 2, 0);
    gfx_pen(TEXT_DARK);
    gfx_textf("%5u %5u %5u %5u", CLOCK_TICKS_TO_US(sample_start),
                            CLOCK_TICKS_TO_US(vblank_end),
                            CLOCK_TICKS_TO_US(frontend_end),
                            CLOCK_TICKS_TO_US(frame_total));
    gfx_end_window();
}

#endif // ML_DEBUG

