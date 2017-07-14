#include "tiny485_syscfg.h"

#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>

struct tiny485_syscfg tiny485_syscfg;

static const PROGMEM
struct tiny485_syscfg tiny485_syscfg_defaults = {
	.nodeaddr = '@',
	.servo = {
		.pwm1 = 1000,
		.pwm2 = 2000,
		.maxcnt = 19999
	}
};


void
tiny485_syscfg_init()
{
	/* FIXME: this needs to be properly thought through, which pins
	 * to use for "reset to defaults" function
	 */
#if 0
	if (PINA & _BV(0)) {
		/* if pin is high, read from eeprom */
#endif
		eeprom_read_block(&tiny485_syscfg, NULL, sizeof(tiny485_syscfg));
#if 0
	} else {
		/* read defaults from flash */
		memcpy_P(&tiny485_syscfg, &tiny485_syscfg_defaults,
			sizeof(struct tiny485_syscfg));
	}
#endif
	tiny485_syscfg.nodeaddr = 0x40;
}

void
tiny485_syscfg_save()
{
	eeprom_update_block(&tiny485_syscfg, NULL, sizeof(tiny485_syscfg));
}
