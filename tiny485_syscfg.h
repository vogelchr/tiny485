#ifndef TINY485_SYSCFG_H
#define TINY485_SYSCFG_H

struct tiny485_syscfg {
	unsigned char nodeaddr;	
};

extern struct tiny485_syscfg tiny485_syscfg;

extern void tiny485_syscfg_init();
extern void tiny485_syscfg_update();

#endif