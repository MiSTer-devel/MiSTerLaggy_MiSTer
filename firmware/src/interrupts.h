#if !defined( INTERRUPTS_H )
#define INTERRUPTS_H 1

__attribute__((interrupt)) void bus_error_handler();
__attribute__((interrupt)) void address_error_handler();
__attribute__((interrupt)) void illegal_instruction_handler();
__attribute__((interrupt)) void zero_divide_handler();
__attribute__((interrupt)) void chk_handler();
__attribute__((interrupt)) void trapv_handler();
__attribute__((interrupt)) void priv_violation_handler();
__attribute__((interrupt)) void trace_handler();
__attribute__((interrupt)) void trap_1010_handler();
__attribute__((interrupt)) void trap_1111_handler();
__attribute__((interrupt)) void uninitialized_handler();
__attribute__((interrupt)) void spurious_handler();
__attribute__((interrupt)) void level1_handler();
__attribute__((interrupt)) void level2_handler();
__attribute__((interrupt)) void level3_handler();
__attribute__((interrupt)) void level4_handler();
__attribute__((interrupt)) void level5_handler();
__attribute__((interrupt)) void level6_handler();
__attribute__((interrupt)) void level7_handler();
__attribute__((interrupt)) void trap0_handler();
__attribute__((interrupt)) void trap1_handler();
__attribute__((interrupt)) void trap2_handler();
__attribute__((interrupt)) void trap3_handler();
__attribute__((interrupt)) void trap4_handler();
__attribute__((interrupt)) void trap5_handler();
__attribute__((interrupt)) void trap6_handler();
__attribute__((interrupt)) void trap7_handler();
__attribute__((interrupt)) void trap8_handler();
__attribute__((interrupt)) void trap9_handler();
__attribute__((interrupt)) void trap10_handler();
__attribute__((interrupt)) void trap11_handler();
__attribute__((interrupt)) void trap12_handler();
__attribute__((interrupt)) void trap13_handler();
__attribute__((interrupt)) void trap14_handler();
__attribute__((interrupt)) void trap15_handler();

static inline void enable_interrupts() { asm( "andi #0xf8ff, %sr" ); }
static inline void disable_interrupts() { asm( "ori #0x0700, %sr" ); }

#endif