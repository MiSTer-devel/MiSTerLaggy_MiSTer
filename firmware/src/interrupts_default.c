#include "interrupts.h"

__attribute__((interrupt,weak)) void bus_error_handler()
{
	while(1) {}
}

__attribute__((interrupt,weak)) void address_error_handler()
{
	while(1) {}
}

__attribute__((interrupt,weak)) void illegal_instruction_handler()
{
	while(1) {}
}

__attribute__((interrupt,weak)) void zero_divide_handler()
{
	return;
}

__attribute__((interrupt,weak)) void chk_handler()
{
	return;
}

__attribute__((interrupt,weak)) void trapv_handler()
{
	return;
}

__attribute__((interrupt,weak)) void priv_violation_handler()
{
	return;
}

__attribute__((interrupt,weak)) void trace_handler()
{
	return;
}

__attribute__((interrupt,weak)) void trap_1010_handler()
{
	return;
}

__attribute__((interrupt,weak)) void trap_1111_handler()
{
	return;
}

__attribute__((interrupt,weak)) void uninitialized_handler()
{
	return;
}

__attribute__((interrupt,weak)) void spurious_handler()
{
	return;
}

__attribute__((interrupt,weak)) void level1_handler()
{
	return;
}

__attribute__((interrupt,weak)) void level2_handler()
{
	return;
}

__attribute__((interrupt,weak)) void level3_handler()
{
	return;
}

__attribute__((interrupt,weak)) void level4_handler()
{
	return;
}

__attribute__((interrupt,weak)) void level5_handler()
{
	return;
}

__attribute__((interrupt,weak)) void level6_handler()
{
	return;
}

__attribute__((interrupt,weak)) void level7_handler()
{
	return;
}

__attribute__((interrupt,weak)) void trap0_handler()
{
	return;
}

__attribute__((interrupt,weak)) void trap1_handler()
{
	return;
}

__attribute__((interrupt,weak)) void trap2_handler()
{
	return;
}

__attribute__((interrupt,weak)) void trap3_handler()
{
	return;
}

__attribute__((interrupt,weak)) void trap4_handler()
{
	return;
}

__attribute__((interrupt,weak)) void trap5_handler()
{
	return;
}

__attribute__((interrupt,weak)) void trap6_handler()
{
	return;
}

__attribute__((interrupt,weak)) void trap7_handler()
{
	return;
}

__attribute__((interrupt,weak)) void trap8_handler()
{
	return;
}

__attribute__((interrupt,weak)) void trap9_handler()
{
	return;
}

__attribute__((interrupt,weak)) void trap10_handler()
{
	return;
}

__attribute__((interrupt,weak)) void trap11_handler()
{
	return;
}

__attribute__((interrupt,weak)) void trap12_handler()
{
	return;
}

__attribute__((interrupt,weak)) void trap13_handler()
{
	return;
}

__attribute__((interrupt,weak)) void trap14_handler()
{
	return;
}

__attribute__((interrupt,weak)) void trap15_handler()
{
	return;
}
