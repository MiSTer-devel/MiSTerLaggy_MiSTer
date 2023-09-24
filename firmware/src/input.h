#if !defined(INPUT_H)
#define INPUT_H 1

#include <stdint.h>

#define INPUT_RIGHT (1 << 0)
#define INPUT_LEFT (1 << 1)
#define INPUT_DOWN (1 << 2)
#define INPUT_UP (1 << 3)
#define INPUT_OK (1 << 4)
#define INPUT_BACK (1 << 5)
#define INPUT_MENU (1 << 6)

void input_poll();
uint16_t input_pressed();
uint16_t input_released();
uint16_t input_state();

#endif