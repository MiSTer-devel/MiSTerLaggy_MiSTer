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
    gfx_begin_region(14, 26, 26, 2);
    gfx_pen(TEXT_DARK_GREEN);
    gfx_textf("%u %u %u %u", CLOCK_TICKS_TO_US(sample_start),
                            CLOCK_TICKS_TO_US(vblank_end),
                            CLOCK_TICKS_TO_US(frontend_end),
                            CLOCK_TICKS_TO_US(frame_total));
    gfx_end_region();
}


