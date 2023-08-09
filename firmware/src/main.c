#include <stdint.h>
#include <stdbool.h>
#include "printf/printf.h"

#include "util.h"
#include "interrupts.h"

/*
void draw_bg_text(TC0100SCN_BG *bg, int color, uint16_t x, uint16_t y, const char *str)
{
    int ofs = ( y * 64 ) + x;

    while(*str)
    {
        if( *str == '\n' )
        {
            y++;
            ofs = (y * 64) + x;
        }
        else
        {
            bg[ofs].attr = 0;
            bg[ofs].color = color & 0xff;
            bg[ofs].code = *str;
            ofs++;
        }
        str++;
    }
}
*/

volatile uint32_t vblank_count = 0;

int main(int argc, char *argv[])
{
    return 0;
}

