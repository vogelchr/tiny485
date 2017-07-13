#include "rs485.h"
#include "tiny485_syscfg.h"

#include <avr/io.h>
#include <avr/cpufunc.h>
#include <avr/interrupt.h>
#include <string.h>

#define CMD_PING  0x01

int
main()
{
	unsigned char msglen;
	unsigned char msgid;

	tiny485_syscfg_init();
	rs485_init();
	sei(); /* enable interrupts */

	rs485_txbuf[0]='!';
	rs485_txbuf[1]=tiny485_syscfg.nodeaddr;
	rs485_start_tx(2);

	while(1) {
		msglen = rs485_poll();
		if (msglen == RS485_POLL_NOMSG)
			continue;

		if (msglen == 0) {
			msgid = CMD_PING;
		} else {
			msgid = rs485_rxbuf[0];
		}

		if (msgid == CMD_PING) {
			memcpy(rs485_txbuf, rs485_rxbuf, msglen);
			rs485_start_tx(msglen);
			goto rxok;
		}

		rs485_txbuf[0] = '?';
		rs485_txbuf[1] = rs485_rxbuf[0]; /* command not understood */
		rs485_start_tx(2);
rxok:
		rs485_rxok();
	}
}
