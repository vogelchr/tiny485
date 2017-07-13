#ifndef RS485_H
#define RS485_H

#define RS485_MAX_MSGSIZE 16

/* receive data buffer */
extern unsigned char rs485_rxbuf[RS485_MAX_MSGSIZE];

/* transmit data buffer */
extern unsigned char rs485_txbuf[RS485_MAX_MSGSIZE];

/* poll returns NOMSG when there is no new message */
#define RS485_POLL_NOMSG 0xff
extern unsigned char rs485_poll();
extern void rs485_rxok();

extern void rs485_start_tx(unsigned char len);

extern void rs485_init();


#endif
