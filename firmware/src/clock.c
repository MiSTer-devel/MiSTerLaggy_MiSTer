#include "clock.h"

void clock_ticks_to_ms_us(uint32_t ticks, uint32_t *ms, uint32_t *us)
{
    *ms = ticks / CLOCK_REF_KHZ;
    *us = (ticks / CLOCK_REF_MHZ ) % 1000;
}