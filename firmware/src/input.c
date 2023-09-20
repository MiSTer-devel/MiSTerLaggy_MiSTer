#include "input.h"

static volatile uint16_t *gamepad_port = (volatile uint16_t *)0x400000;


static uint16_t gamepad_prev = 0;
static uint16_t gamepad;
static uint16_t gamepad_pressed;
static uint16_t gamepad_released;


void input_poll()
{
    gamepad = *gamepad_port;

    gamepad_pressed = (gamepad ^ gamepad_prev) & gamepad;
    gamepad_released = (gamepad ^ gamepad_prev) & gamepad_prev;

    gamepad_prev = gamepad;
}

uint16_t input_pressed()
{
    return gamepad_pressed;
}

uint16_t input_state()
{
    return gamepad;
}

uint16_t input_released()
{
    return gamepad_released;
}

