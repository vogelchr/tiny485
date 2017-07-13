#include "rs485.h"
#include "tiny485_syscfg.h"

#include <avr/io.h>
#include <avr/cpufunc.h>
#include <avr/interrupt.h>
#include <string.h>

#define CMD_PING        0x01
#define CMD_SERVO       0x02
#define CMD_QUERY_SERVO 0x03

static void
servo_pwm_init()
{
	/* Timer/Counter 1, with a 1/8 prescaler, servo duty time is
	    20ms = 20000 usec */

	ICR1 = 19999;
	OCR1A = 1000; /* 1ms */
	OCR1B = 2000; /* 2ms */

	/* Fast PWM Mode, counting 0..ICR1:
	   WGM13=1, WGM12=1, WGM11=1, WGM10=0

	   clear OC1A/OC1B on compare match, set on ZERO:
	   COM1x1=1, COM1x0=0

	   clock prescaler set to clkIO/8
	*/

	TCCR1A = _BV(COM1A1)|_BV(COM1B1)|_BV(WGM11); /* WGM10=0 */
	TCCR1B = _BV(CS11)|_BV(WGM13)|_BV(WGM12);    /* CS12=0, CS11=1, CS10=0 */
	DDRB  |= _BV(4); /* PB4 = OC1B */
	DDRB  |= _BV(3); /* PB3 = OC1A */
}


ISR(TIMER0_OVF_vect) {
	PORTB ^= _BV(0);
}

static void
clock_tick_init()
{
	/* 125 cycles @ 8 MHz / 64 = 1ms */
	OCR0A = 124;
	/* fast PWM mode, TOP=OCR0A, TOV flag set on TOP */
	TCCR0A = _BV(WGM01)|_BV(WGM00);
	TCCR0B = _BV(WGM02)|_BV(CS01)|_BV(CS00);  /* clkIO/64 */
	TIMSK = _BV(TOIE0); /* timer overflow interrupt enable */
	DDRB |= _BV(0);
}

int
main()
{
	unsigned char msglen;
	unsigned char msgid;

	tiny485_syscfg_init();
	rs485_init();
	servo_pwm_init();
	clock_tick_init();

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

		if (msgid == CMD_SERVO) {
			/* always write high first, low second, there's a
			   temp storage register for the high byte, and the
			   16bit storage is flushed when the low byte is written */
			if (msglen < 2)
				goto rxok;
			OCR1AH = rs485_rxbuf[1];
			OCR1AL = rs485_rxbuf[0];
			if (msglen < 4)
				goto rxok;
			OCR1BH = rs485_rxbuf[3];
			OCR1BL = rs485_rxbuf[2];
			if (msglen < 6)
				goto rxok;
			ICR1H = rs485_rxbuf[5];
			ICR1L = rs485_rxbuf[4];
			goto rxok;
		}

		if (msgid == CMD_QUERY_SERVO) {
			rs485_txbuf[0] = OCR1AL;
			rs485_txbuf[1] = OCR1AH;
			rs485_txbuf[2] = OCR1BL;
			rs485_txbuf[3] = OCR1BH;
			rs485_txbuf[4] = ICR1L;
			rs485_txbuf[5] = ICR1H;
			rs485_start_tx(6);
			goto rxok;
		}

		rs485_txbuf[0] = '?';
		rs485_txbuf[1] = rs485_rxbuf[0]; /* command not understood */
		rs485_start_tx(2);
rxok:
		rs485_rxok();
	}
}
