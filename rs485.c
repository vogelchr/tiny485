#include "rs485.h"
#include "tiny485_syscfg.h"
#include <avr/io.h>
#include <avr/interrupt.h>

#define BAUD 9600
#define USE_U2X 1
#define UBRR_CALC(baud,f_osc,u2x) (((uint32_t)(f_osc)/(8*(2-(u2x))*(uint32_t)(baud)))-1)

/* inquiry:
     <SOH> <addr> <payload> <EOT>
   answer:
     <STX> <payload> <ETX>
*/

enum ASCII_CHARS {
	ASCII_SOH = '\001',
	ASCII_STX = '\002',
	ASCII_ETX = '\003',
	ASCII_EOT = '\004',
	ASCII_ESC = '\033',
	ASCII_LOW = '\040'  /* escape everything < ASCII_LOW */
};

/*
 *                   +-------+
 *                   |       |
 *                   v       | data(ignored)
 *   RS485_RX_SKIP_REPLY ----+
 *    |      ^
 *    | ETX  | STX
 *    |      |
 *    +--> IDLE  <-------------------+<--------------------+
 *           |                       |      +-----+        |
 *           | SOH               EOT |      |     |        |
 *           v                       |      v     | data   |
 *        RS485_RX_ADDR  ---> RS485_RX_PAYLOAD ---+ (store)|
 *           |           addr                              |
 *           |                                        EOT  |
 *           +--------------> RS485_RX_SKIP_PAYLOAD -------+
 *                    != addr      |   ^
 *                                 |   | data(ignored)
 *                                 +---+
 *
 * Any STX received jumps directly to SKIP_REPLY.
 * Any SOH received jumps directly to RX_ADDR.
 * Any EOT, ETX, or undefined character < 0x20 jumps directly to IDLE.
 * To allow data or addr < 0x20 to be transmitted, it has to be escaped
 * by ESC (0x13, dec 27, \033) and the character be XORed with 0x20.
 * e.g. <0x01> is transmitted as <0x13 0x21>.
 * 
 */

#define RS485_ESC_FLAG 0x80  /* set this bit in flags if ESC detected */
enum rs485_rx_state {
	RS485_RX_IDLE,
	RS485_RX_ADDR,
	RS485_RX_PAYLOAD,
	RS485_RX_SKIP_PAYLOAD,
	RS485_RX_SKIP_REPLY,
};

unsigned char rs485_rxbuf[RS485_MAX_MSGSIZE];
#define RS485_RXBUFP_BUSY 0x80
static unsigned char rs485_rxbufp;

enum rs485_rx_state rs485_rx_state;

/* receive complete */
ISR(USART_RX_vect)
{
	enum rs485_rx_state state = rs485_rx_state;
	unsigned char p = rs485_rxbufp;
	unsigned char c;

	c = UDR;

	switch (c) {
	case ASCII_SOH:
		/* state != RS485_RX_IDLE error, ignored */
		state = RS485_RX_ADDR;
		break;
	case ASCII_EOT:
		/* state != RS485_RX_PAYLOAD && state != RS485_RX_SKIP_PAYLOAD
		   would be an error, but is ignored */
		/* only mark message as received when it was addressed to us */
		if (state == RS485_RX_PAYLOAD)  /* mark messages as received */
			p |= RS485_RXBUFP_BUSY;
		state = RS485_RX_IDLE;
		break;
	case ASCII_STX:
		/* state != RS485_RX_IDLE would be an error, ignored */
		state = RS485_RX_SKIP_REPLY;
		break;
	case ASCII_ETX:
		/* state != RS485_RX_SKIP_REPLY would be an error, ignored */
		state = RS485_RX_IDLE;
		break;
	case ASCII_ESC:
		state |= RS485_ESC_FLAG;
		break;
	default:
		/* received a non-control character, only valid during address */
		/* or payload, check if we had a ESC just before this character */
		if (state & RS485_ESC_FLAG) {
			c ^= ASCII_LOW;
			state &= ~RS485_ESC_FLAG;
		}
		/* handle address, payload, skip_payload or skip_reply */
		if (state == RS485_RX_ADDR) {
			/* our address, and we are not still busy */
			if (c == tiny485_syscfg.nodeaddr && !(p & RS485_RXBUFP_BUSY))
			{
				state = RS485_RX_PAYLOAD;
				p = 0;
			} else {
				state = RS485_RX_SKIP_PAYLOAD;
			}
		} else if (state == RS485_RX_PAYLOAD) {
			if (p >= RS485_MAX_MSGSIZE) {
				/* flag error */
				state = RS485_RX_IDLE;
			} else {
				rs485_rxbuf[p++] = c;
			}
		} else if (state == RS485_RX_SKIP_PAYLOAD ||
			       state == RS485_RX_SKIP_REPLY
		) {
			/* ok, just ignore the payload */
		} else {
			/* received non-character while not waiting for
			   address or payload */
			state = RS485_RX_IDLE;
		}
	}

	rs485_rx_state = state;
	rs485_rxbufp = p;
}

unsigned char rs485_poll() {
	unsigned char p;

	cli();
	p = rs485_rxbufp;
	if (p & RS485_RXBUFP_BUSY) {
		rs485_rxbufp = 0;
		sei();
		return p & ~RS485_RXBUFP_BUSY;
	}
	sei();
	return 0;
}

enum rs485_tx_state {
	RS485_TX_STX,
	RS485_TX_PAYLOAD,
	RS485_TX_ESC,
	RS485_TX_ETX,
	RS485_TX_END
};

unsigned char rs485_txbuf[RS485_MAX_MSGSIZE];
static enum rs485_tx_state rs485_tx_state;
static unsigned char rs485_txbuflen;
static unsigned char rs485_txbufp;

/* transmit complete */
ISR(USART_TX_vect)
{
	enum rs485_tx_state state = rs485_tx_state;
	if (state == RS485_TX_END)
		PORTA &= ~_BV(1);     /* disable RS485 TX */
}

/* data register empty */
ISR(USART_UDRE_vect)
{
	enum rs485_tx_state state = rs485_tx_state;
	unsigned char l = rs485_txbuflen;
	unsigned char p = rs485_txbufp;
	unsigned char c;

	switch (state) {
	case RS485_TX_STX:
		PORTA |= _BV(1); /* enable RS485 TX */
		c = ASCII_STX;
		p = 0;
		goto state_after_payload_byte;
	case RS485_TX_PAYLOAD:
	case RS485_TX_ESC:
		c = rs485_txbuf[p];
		if (state == RS485_TX_ESC) {
			c ^= ASCII_LOW;
		} else {
			if (c < ASCII_LOW) {
				state = RS485_TX_ESC;
				c = ASCII_ESC;
			} else {
				p++;
			}
		}
state_after_payload_byte:
		if (p >= l)
			state = RS485_TX_ETX;
		else
			state = RS485_TX_PAYLOAD;
		break;
	case RS485_TX_ETX:
		c = ASCII_ETX;
		state = RS485_TX_END;
		break;
	case RS485_TX_END:
		l = 0;  /* mark buffer as sent */
		UCSRB &= ~_BV(UDRIE); /* disable data register empty interrupt */
		goto no_write_udr;
	}
	UDR = c;
no_write_udr:
	rs485_tx_state = state;
	rs485_txbufp = p;
	rs485_txbuflen = l;
}

void
rs485_start_tx(unsigned char len)
{
	cli();
	rs485_tx_state = RS485_TX_STX; /* set to start tx */
	rs485_txbuflen = len;
	UCSRB |=  _BV(UDRIE); /* enable data register empty interrupt */
	sei();
}

void
rs485_init()
{
	uint16_t ubrr = UBRR_CALC(BAUD,F_CPU,USE_U2X);

	/* PD0 = RxD, PD1 = TxD */
	DDRD  &= ~_BV(0);  /* PD0 = input */
	PORTD |=  _BV(0);  /* weak pullup on RxD */

	/* PA1 = RX485 Tx Enable */
	DDRA  |=  _BV(1);   /* PA1 = RS485 Tx Enable */
	PORTA &= ~_BV(1);  /* disable RS485 TX */

	UBRRL =  ubrr       & 0x00ff;
	UBRRH = (ubrr >> 8) & 0x00ff;
	UCSRA = _BV(TXC) | (USE_U2X?_BV(U2X):0); /* clear tx complete interrupt, just in case */
	UCSRB = 0;
	UCSRC = _BV(UCSZ1)|_BV(UCSZ0); /* 8 bit */
	UCSRB |= _BV(TXCIE)|_BV(RXCIE)|_BV(RXEN)|_BV(TXEN);
}
