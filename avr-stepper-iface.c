#include "rs485.h"
#include "tiny485_syscfg.h"

#include <avr/io.h>
#include <avr/cpufunc.h>
#include <avr/interrupt.h>

int
main()
{
	uint16_t v = 0x1234;

	tiny485_syscfg_init();
	rs485_init();
	sei(); /* enable interrupts */

	while(1) {
		if (rs485_poll() == RS485_POLL_NOMSG)
			continue;
		if (rs485_rxbuf[0] == 0x01)
			PORTA |= _BV(0);
		if (rs485_rxbuf[0] == 0x02)
			PORTA &= ~_BV(0);
		if (rs485_rxbuf[0] == 0x03) {
			rs485_txbuf[0] =  v       & 0xff;
			rs485_txbuf[1] = (v >> 8) & 0xff;
			rs485_start_tx(2);
		}

	}

}
