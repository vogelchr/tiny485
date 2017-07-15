#include "tiny485_syscfg.h"

#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <stdint.h>

#ifdef __AVR_ATtiny2313__
#define EEPROM_SIZE 128
#else
#error Unknown eeprom size.
#endif

#define EEPROM_PTR_MAGIC (uint8_t *)(EEPROM_SIZE-1)
#define EEPROM_VAL_MAGIC 0xa5

struct tiny485_syscfg tiny485_syscfg;

static const PROGMEM
struct tiny485_syscfg tiny485_syscfg_defaults = {
	.nodeaddr = '@',
	.servo = {
		.pwm1 = 1000,
		.pwm2 = 2000,
		.maxcnt = 19999
	},
	.stepper = {
		.minpos = 96,
		.maxpos = 7680
	}
};


void
tiny485_syscfg_init()
{
	uint8_t eeprom_magic;

	eeprom_magic = eeprom_read_byte(EEPROM_PTR_MAGIC);		
	if (eeprom_magic == 0xa5) {
		eeprom_read_block(&tiny485_syscfg, NULL, sizeof(tiny485_syscfg));
	} else {
		memcpy_P(&tiny485_syscfg, &tiny485_syscfg_defaults,
			sizeof(struct tiny485_syscfg));
		tiny485_syscfg_save();
		eeprom_write_byte(EEPROM_PTR_MAGIC, EEPROM_VAL_MAGIC);
	}
}

void
tiny485_syscfg_save()
{
	eeprom_update_block(&tiny485_syscfg, NULL, sizeof(tiny485_syscfg));
}
