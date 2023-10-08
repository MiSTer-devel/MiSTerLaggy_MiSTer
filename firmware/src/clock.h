#if !defined( CLOCK_H )
#define CLOCK_H 1

#include <stdint.h>

#include "interrupts.h"

#define CLOCK_REF_HZ 10000000
#define CLOCK_REF_KHZ 10000
#define CLOCK_REF_MHZ 10

#define CLOCK_MS_TO_TICKS(ms) ((ms) * CLOCK_REF_KHZ)
#define CLOCK_TICKS_TO_MS(ticks) ((ticks) / CLOCK_REF_KHZ)

void clock_ticks_to_ms_us(uint32_t ticks, uint32_t *ms, uint32_t *us);


typedef volatile struct
{
    uint32_t value;
    uint16_t latch;
} ClockTicks;


static inline uint32_t clock_get_ticks()
{
    ClockTicks *ticks = (ClockTicks *)0x200000;

    disable_interrupts();
    ticks->latch = 0xffff;
    uint32_t res = ticks->value;
    enable_interrupts();

    return res;
}

#endif // CLOCK_H