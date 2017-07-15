#include "stepper.h"
#include "tiny485_syscfg.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdint.h>

/*
 * Coil 1 is between pins 3 and 4.
 * Coil 2 is between pins 1 and 2.
 *
 * The documentation of the motor defines a driving
 * waveform that always sets pins 2+3 to the same level.
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

enum stepper_state {
	STEPPER_OFF = 0,
	STEPPER_ON  = 1,
	STEPPER_ZEROING = 2
};
enum stepper_state stepper_state;

static char stepper_micstep;

uint16_t    stepper_target;
uint16_t    stepper_pos;
static uint8_t stepper_ramp;

#define MICSTEP_SHIFT 3
#define MICSTEP_MAX   (sizeof(stepper_step_table) << MICSTEP_SHIFT)

static void
stepper_advance(int8_t delta)
{
	uint8_t micstep;
	int8_t sstep;
	uint8_t v, ix;

	micstep = stepper_micstep;
	sstep = ((int8_t)micstep) + delta;
	while (sstep < 0)
		sstep += MICSTEP_MAX;
	while (sstep >= MICSTEP_MAX)
		sstep -= MICSTEP_MAX;
	micstep = sstep;

	if (stepper_state == STEPPER_OFF)
		v = 0;
	else {
		ix = micstep >> MICSTEP_SHIFT;
		v = pgm_read_byte(stepper_step_table+ix);
	}

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
	uint16_t target, pos, delta;
	uint8_t  ramp;
	enum stepper_state state;

	target = stepper_target;
	pos = stepper_pos;
	ramp = stepper_ramp;
	state = stepper_state;

	if (target == pos && state == STEPPER_ZEROING) {
		target = tiny485_syscfg.stepper.minpos;
		stepper_target = target;
		state = STEPPER_ON;
	}

	/* get absolute delta */
	if (pos < target) {
		delta = target - pos;
	} else
		delta = pos - target;

	/* ramp increases, and decreases as we reach the target */
	if (delta < (uint16_t)ramp)
		ramp = delta;
	else if (ramp < 0xff)
		ramp++;
	stepper_ramp = ramp;

	ramp >>= (8-MICSTEP_SHIFT);
	ramp++;

	if (pos < target) {
		dir = ramp;
		pos += ramp;
	} else if (pos > target) {
		dir = -ramp;
		pos -= ramp;
	}

	stepper_advance(dir);
	stepper_pos = pos;
	stepper_state = state;
}

void
stepper_init()
{
	DDR_P1 |= BIT_P1;
	DDR_P2 |= BIT_P2;
	DDR_P3 |= BIT_P3;
	DDR_P4 |= BIT_P4;

	stepper_state = STEPPER_ZEROING;
	stepper_target = 0;
	stepper_pos = tiny485_syscfg.stepper.maxpos + 96;

//	OCR0A = 124; /* 125 cycles @ 8 MHz / 64 = 1ms */
	OCR0A =  62; /*  63 cycles @ 8 MHz / 64 = 0.5 ms */
	/* fast PWM mode, TOP=OCR0A, TOV flag set on TOP */
	TCCR0A = _BV(WGM01)|_BV(WGM00);
	TCCR0B = _BV(WGM02)|_BV(CS01)|_BV(CS00);  /* clkIO/64 */
//	TCCR0B = _BV(WGM02)|_BV(CS02);            /* clkIO/256 */

	TIMSK = _BV(TOIE0); /* timer overflow interrupt enable */
	DDRB |= _BV(0);
}


void
stepper_goto(uint16_t g)
{
	cli();
	/* ignore while zeroing */
	if (stepper_state == STEPPER_ZEROING)
		return;
	if (g > tiny485_syscfg.stepper.maxpos)
		g = tiny485_syscfg.stepper.maxpos;
	if (g < tiny485_syscfg.stepper.minpos )
		g = tiny485_syscfg.stepper.minpos;

	stepper_state = STEPPER_ON;
	stepper_target = g;
	sei();
}

void
stepper_zero(uint16_t v)
{
	cli();
	stepper_state = STEPPER_ZEROING;
	stepper_target = 0;
	stepper_pos = tiny485_syscfg.stepper.maxpos + 96;
	sei();
}

void
stepper_off()
{
	cli();
	stepper_target = stepper_pos;
	stepper_state = STEPPER_OFF;
	/* also used for init */
	PORT_P1 &= ~BIT_P1;
	PORT_P2 &= ~BIT_P2;
	PORT_P3 &= ~BIT_P3;
	PORT_P4 &= ~BIT_P4;	
	sei();
}
