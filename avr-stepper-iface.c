#include "rs485.h"
#include "tiny485_syscfg.h"
#include "stepper.h"

#include <avr/io.h>
#include <avr/cpufunc.h>
#include <avr/interrupt.h>
#include <string.h>

/*
 *                      +----u----+
 *            \Reset .. | 1    20 | -- Vcc
 *         RS485-RxD D0 | 2    19 | B7 SCL
 *         RS485-TxD D1 | 3    18 | B6 MISO
 *       Rotary-in-3 A1 | 4    17 | B5 MOSI
 *       Rotary-in-4 A0 | 5    16 | B4 Servos-5
 *      RS485-DR/\RE D2 | 6    15 | B3 Servos-6
 *         Stepper-4 D3 | 7    14 | B2 Led-Mosfet-G
 *         Stepper-4 D3 | 7    14 | B2 Led-Mosfet-G
 *                   D4 | 8    13 | B1 Stepper-1
 *                   D5 | 9    12 | B0 Stepper-2
 *               GND -- | 10   11 | D6 Led-Out-5
 *                      +---------+
 *
 */

enum AVR_STEPPER_IFACE_CMDS {
	CMD_PING         = 'P',
	CMD_SERVO        = 'S', 
	CMD_QUERY_SERVO  = 'Q',
	CMD_SET_NODEADDR = 'A',
	CMD_SAVE_CONFIG  = 'C',
	CMD_STEPPER_GOTO = 'G',
	CMD_STEPPER_ZERO = 'Z',
	CMD_STEPPER_OFF  = 'O',
	CMD_STEPPER_GET  = 'E'
};

static void
servo_pwm_update()
{
	ICR1 = tiny485_syscfg.servo.maxcnt;
	OCR1A = tiny485_syscfg.servo.pwm1; /* 1ms */
	OCR1B = tiny485_syscfg.servo.pwm2; /* 2ms */
}

static void
servo_pwm_init()
{
	/* Timer/Counter 1, with a 1/8 prescaler, servo duty time is
	    20ms = 20000 usec */

	servo_pwm_update();

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

int
main()
{
	unsigned char msglen;
	unsigned char msgid;

	tiny485_syscfg_init();
	rs485_init();
	servo_pwm_init();
	stepper_init();

	sei(); /* enable interrupts */

	/* main message handler loop */
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

		/* msglen is everything besides the first byte, but PING copies
		   back the whole message, so we only subtract here... */
		msglen--;

		/* craft default reply for ack: <nodeaddr> <msgid> */
		rs485_txbuf[0] = tiny485_syscfg.nodeaddr;
		rs485_txbuf[1] = msgid;

		/* CMD_SERVO + up to 6 bytes of syscfg.servo data */
		if (msgid == CMD_SERVO) {
			/* must be <= 6 bytes and an even number of bytes */
			if (msglen > sizeof(tiny485_syscfg.servo) || (msglen & 0x01))
				goto invalidcmd;
			memcpy(&tiny485_syscfg.servo, rs485_rxbuf+1, msglen);
			/* no reply, no need to hog the bus */
			servo_pwm_update();
			goto rxok;
		}

		if (msgid == CMD_QUERY_SERVO) {
			memcpy(rs485_txbuf+2, &tiny485_syscfg.servo,
				sizeof(tiny485_syscfg.servo));
			rs485_start_tx(2+sizeof(tiny485_syscfg.servo));
			goto rxok;
		}

		if (msgid == CMD_SET_NODEADDR) {
			unsigned char o,a,b;
			if (msglen != 3)
				goto invalidcmd;
			o = ~rs485_rxbuf[1]; // inverse of old addr
			a =  rs485_rxbuf[2]; // new addr
			b = ~rs485_rxbuf[3]; // inverse of new addr
			if (o != tiny485_syscfg.nodeaddr || a != b)
				goto invalidcmd; /* safety */
			tiny485_syscfg.nodeaddr = a;
			rs485_start_tx(2); /* default ack reply */
			goto rxok;
		}

		if (msgid == CMD_SAVE_CONFIG) {
			unsigned char o;
			if (msglen != 1)
				goto invalidcmd;
			o = ~rs485_rxbuf[1]; // inverse of old addr
			if (o != tiny485_syscfg.nodeaddr)
				goto invalidcmd; /* safety */
			tiny485_syscfg_save();
			rs485_start_tx(2); /* default ack reply */
			goto rxok;
		}

		if (msgid == CMD_STEPPER_GOTO) {
			uint16_t v;
			if (msglen != 2)
				goto invalidcmd;
			v = (rs485_rxbuf[2] << 8)|rs485_rxbuf[1]; /* big end. */
			stepper_goto(v);
			goto rxok;
		}

		if (msgid == CMD_STEPPER_ZERO) {
			uint16_t v;
			if (msglen != 2)
				goto invalidcmd;
			v = (rs485_rxbuf[2] << 8)|rs485_rxbuf[1]; /* little end. */
			stepper_zero(v);
			rs485_start_tx(2); /* default ack reply */
			goto rxok;
		}

		if (msgid == CMD_STEPPER_OFF) {
			stepper_off();
			rs485_start_tx(2); /* default ack reply */
			goto rxok;
		}

		if (msgid == CMD_STEPPER_GET) {
			cli();
			rs485_txbuf[2] =  stepper_pos & 0xff;
			rs485_txbuf[3] = (stepper_pos >> 8) & 0xff;
			rs485_txbuf[4] =  stepper_target & 0xff;
			rs485_txbuf[5] = (stepper_target >> 8) & 0xff;
			sei();
			rs485_start_tx(6);
			goto rxok;
		}

invalidcmd:
		/* txbuf[0] == nodeaddr (default, see above) */
		rs485_txbuf[1] = '?';   /* reply with '?' instead of msgid */
		rs485_txbuf[2] = msgid; /* command not understood */
		rs485_start_tx(3);
rxok:
		rs485_rxok();
	}
}
