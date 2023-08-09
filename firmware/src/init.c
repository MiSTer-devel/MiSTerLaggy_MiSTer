#include <stdint.h>
#include <stddef.h>

#include "interrupts.h"

/* These are defined in the linker script */

extern uint16_t _stext;
extern uint16_t _etext;
extern uint16_t _sbss;
extern uint16_t _ebss;
extern uint16_t _sdata;
extern uint16_t _edata;
extern uint16_t _sstack;
extern uint16_t _estack;

/* Forward define main */
int main(void);

static void reset_handler(void)
{
    /* Copy init values from text to data */
    uint16_t *init_values_ptr = &_etext;
    uint16_t *data_ptr = &_sdata;

    if (init_values_ptr != data_ptr)
    {
        for (; data_ptr < &_edata;)
        {
            *data_ptr++ = *init_values_ptr++;
        }
    }

    /* Clear the zero segment */
    for (uint16_t *bss_ptr = &_sbss; bss_ptr < &_ebss;)
    {
        *bss_ptr++ = 0;
    }

    /* Branch to main function */
    main();

    /* Infinite loop */
    while (1);
}

__attribute__ ((section(".vectors"))) __attribute__((used))
static const void *exception_vectors[256] =
{
    &_estack,
    reset_handler,
    bus_error_handler,
    address_error_handler,
    illegal_instruction_handler,
    zero_divide_handler,
    chk_handler,
    trapv_handler,
    priv_violation_handler,
    trace_handler,
    trap_1010_handler,
    trap_1111_handler,
    NULL,
    NULL,
    NULL,
    uninitialized_handler,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    spurious_handler,
    level1_handler,
    level2_handler,
    level3_handler,
    level4_handler,
    level5_handler,
    level6_handler,
    level7_handler,
    trap0_handler,
    trap1_handler,
    trap2_handler,
    trap3_handler,
    trap4_handler,
    trap5_handler,
    trap6_handler,
    trap7_handler,
    trap8_handler,
    trap9_handler,
    trap10_handler,
    trap11_handler,
    trap12_handler,
    trap13_handler,
    trap14_handler,
    trap15_handler,
    NULL
};

void putchar_(char c) {}
