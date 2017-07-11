#include "tiny485_syscfg.h"

#include <avr/io.h>
#include <avr/eeprom.h>

struct tiny485_syscfg tiny485_syscfg;


void
tiny485_syscfg_init()
{
	eeprom_read_block(&tiny485_syscfg, NULL, sizeof(tiny485_syscfg));
}

void
tiny485_syscfg_save()
{
	eeprom_update_block(&tiny485_syscfg, NULL, sizeof(tiny485_syscfg));
}
