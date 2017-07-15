#ifndef TINY485_SYSCFG_H
#define TINY485_SYSCFG_H

#include <stdint.h>

struct tiny485_syscfg_stepper {
	uint16_t minpos; /* after zeroing, we will go to minpos */
	uint16_t maxpos;
};

struct tiny485_syscfg_servo {
	uint16_t pwm1, pwm2, maxcnt;
};

struct tiny485_syscfg {
	unsigned char nodeaddr;
	struct tiny485_syscfg_servo servo;
	struct tiny485_syscfg_stepper stepper;
};

extern struct tiny485_syscfg tiny485_syscfg;

extern void tiny485_syscfg_init();
extern void tiny485_syscfg_save();

#endif
