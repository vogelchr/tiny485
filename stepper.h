#ifndef STEPPER_H
#define STEPPER_H

#include <stdint.h>

extern uint16_t    stepper_target;
extern uint16_t    stepper_pos;

extern void stepper_init();
extern void stepper_goto(uint16_t v); /* goto position */
extern void stepper_zero(uint16_t v); /* zero that many steps to the left */
extern void stepper_off();            /* turn off coils */
#endif