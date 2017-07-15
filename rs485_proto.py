#!/usr/bin/python


SOH = b'\001'
STX = b'\002'
ETX = b'\003'
EOT = b'\004'
ESC = b'\033'

RXSTATE_IDLE = 0
RXSTATE_PAYLOAD = 2
RXSTATE_ESC = 3


def encode_bytes(b) :
    ba = bytearray()
    for c in b :
        if c < 0x20 :
            ba.append(0x1b)
            ba.append(c ^ 0x20)
        else :
            ba.append(c)
    return bytes(ba) 

class RS485_Interface :

    def __init__(self, tty) :
        self.tty = tty
        self.read_state = 0
        self.rxbuf = bytearray()
        self.rxstate = 0

    def transmit(self, id, msg) :
        d = SOH + encode_bytes([id]) + encode_bytes(msg) + EOT
        self.tty.write(d)

    def poll(self) :
        while True :
            c = self.tty.read(1)
            if not c :
                return
            if c == STX :
                if self.rxstate != RXSTATE_IDLE :
                    print('Received STX while not idle!')
                self.rxbuf.clear()
                self.rxstate = RXSTATE_PAYLOAD
            elif c == ETX :
                if self.rxstate != RXSTATE_PAYLOAD :
                    print('Received ETX while not waiting for payload!')
                self.rxstate = RXSTATE_IDLE
                return bytes(self.rxbuf)
            elif c == ESC :
                if self.rxstate != RXSTATE_PAYLOAD :
                    print('Received ESC while not waiting for payload!')
                    self.rxstate = RXSTATE_IDLE
                self.rxstate = RXSTATE_ESC
            else :
                if self.rxstate == RXSTATE_ESC :
                    self.rxbuf.append( c[0] ^ 0x20 )
                    self.rxstate = RXSTATE_PAYLOAD
                elif self.rxstate == RXSTATE_PAYLOAD :
                    self.rxbuf += c
                else :
                    print('Character', c[0], 'unexpected!')
        return None

