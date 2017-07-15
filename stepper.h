#ifndef STEPPER_H
#define STEPPER_H

#include <stdint.h>

extern void stepper_init();
extern void stepper_goto(uint16_t v);

#endif