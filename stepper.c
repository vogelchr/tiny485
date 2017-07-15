#include "stepper.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdint.h>

/*
 * Coil 1 is between pins 3 and 4.
 * Coil 2 is between pins 1 and 2.
 *
 * Motor : Tiny 2313 : AVR Port
 * ------:-----------:---------
 * Pin 1   Pin 13    : PORTB1
 * Pin 2   Pin 12    : PORTB0
 * Pin 3   Pin 8     : PORTD4
 * Pin 4   Pin 7     : PORTD3
 */

#define PORT_P1  PORTB
#define DDR_P1   DDRB
#define BIT_P1   _BV(1)
#define PORT_P2  PORTB
#define DDR_P2   DDRB
#define BIT_P2   _BV(0)
#define PORT_P3  PORTD
#define DDR_P3   DDRD
#define BIT_P3   _BV(4)
#define PORT_P4  PORTD
#define DDR_P4   DDRD
#define BIT_P4   _BV(3)

#define PIN1_MASK  0b100
#define PIN23_MASK 0b010
#define PIN4_MASK  0b001

static const unsigned char PROGMEM stepper_step_table[] = {
	       /* Pin1 Pin2+3 Pin4 */
	0b101, /*  1    0      1   */
	0b100, /*  1    0      0   */
	0b110, /*  1    1      0   */
	0b010, /*  0    1      0   */
	0b011, /*  0    1      1   */
	0b001, /*  0    0      1   */
};

uint16_t    stepper_target;
uint16_t    stepper_pos;
static char stepper_micstep;

static void
stepper_advance(signed char dir)
{
	unsigned char micstep = stepper_micstep;
	unsigned char v;

	if (dir == 1) {
		micstep++;
		if (micstep == sizeof(stepper_step_table))
			micstep=0;
	} else if (dir < 0) {
		if (micstep == 0)
			micstep = sizeof(stepper_step_table)-1;
		else
			micstep--;
	} else
		return;

	v = pgm_read_byte(stepper_step_table+micstep);
	if (v & PIN1_MASK)
		PORT_P1 |= BIT_P1;
	else
		PORT_P1 &= ~BIT_P1;
	if (v & PIN23_MASK) {
		PORT_P2 |= BIT_P2;
		PORT_P3 |= BIT_P3;
	} else {
		PORT_P2 &= ~BIT_P2;
		PORT_P3 &= ~BIT_P3;		
	}
	if (v & PIN4_MASK)
		PORT_P4 |= BIT_P4;
	else
		PORT_P4 &= ~BIT_P4;

	stepper_micstep = micstep;
}

ISR(TIMER0_OVF_vect) {
	signed char dir=0;
	uint16_t target, pos;

	target = stepper_target;
	pos = stepper_pos;

	if (target>pos)
		dir=1;
	else if (pos<target)
		dir=-1;
	else
		dir=0;

	stepper_advance(dir);
	pos += dir;

	stepper_pos = pos;
}

void
stepper_init()
{
	DDR_P1 |= BIT_P1;
	PORT_P1 &= ~BIT_P1;
	PORT_P2 &= ~BIT_P2;
	PORT_P3 &= ~BIT_P3;
	PORT_P4 &= ~BIT_P4;
	DDR_P2 |= BIT_P2;
	DDR_P3 |= BIT_P3;
	DDR_P4 |= BIT_P4;

	/* 125 cycles @ 8 MHz / 64 = 1ms */
	OCR0A = 124;
	/* fast PWM mode, TOP=OCR0A, TOV flag set on TOP */
	TCCR0A = _BV(WGM01)|_BV(WGM00);
	TCCR0B = _BV(WGM02)|_BV(CS01)|_BV(CS00);  /* clkIO/64 */
	TIMSK = _BV(TOIE0); /* timer overflow interrupt enable */
	DDRB |= _BV(0);
}


void
stepper_goto(uint16_t g)
{
	cli();
	stepper_target = g;
	sei();
}
