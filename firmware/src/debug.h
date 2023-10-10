#if !defined(DEBUG_H)
#define DEBUG_H 1

#define ML_DEBUG 1

#include <stdint.h>
#include "clock.h"

#if defined(ML_DEBUG)

typedef struct
{
    uint32_t vblank_start;
    uint32_t vblank_end;
    uint32_t sample_start;
    uint32_t frontend_end;

    uint32_t frame_total;
} DebugStats;

extern DebugStats debug_stats;

#define DEBUG_FRAME_MARKER(name) debug_stats.name = clock_get_ticks()
#define DEBUG_END_FRAME() debug_end_frame()
#define DEBUG_DRAW() debug_draw()
void debug_end_frame();
void debug_draw();

#else // ML_DEBUG

#define DEBUG_FRAME_MARKER(name) (void)0
#define DEBUG_END_FRAME() (void)0
#define DEBUG_DRAW() (void)0

#endif // ML_DEBUG

#endif // DEBUG_H