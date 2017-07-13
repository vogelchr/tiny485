#!/usr/bin/python
import time
import serial
import datetime
import random
import binascii
import os

SOH = b'\001'
STX = b'\002'
ETX = b'\003'
EOT = b'\004'
ESC = b'\033'

def nowstr() :
    return datetime.datetime.now().strftime('%H:%M:%S.%f')

def encode_bytes(b) :
    ba = bytearray()
    for c in b :
        if c < 0x20 :
            ba.append(0x1b)
            ba.append(c ^ 0x20)
        else :
            ba.append(c)
    return bytes(ba) 

RXSTATE_IDLE = 0
RXSTATE_PAYLOAD = 2
RXSTATE_ESC = 3

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


def maybe_write_junk(tty) :
    if random.randint(0, 10) == 10 :
        # write random junk
        print('Writing random junk.')
        tty.write(os.urandom(random.randint(0,15)))

def main() :
    tty = serial.Serial('/dev/ttyUSB0', 57600, timeout=0.05)
    rs485 = RS485_Interface(tty)

    chipid = 0
    lastmsg = bytes()
    sent_msg = False
    recvd_msg = False

    cnt_noreply = 0
    cnt_msgnoneq = 0
    cnt_msgok = 0

    last_tx = datetime.datetime.now()
    last_stat = last_tx

    while True :
        now = datetime.datetime.now()

        dt = now - last_stat
        if dt.total_seconds() > 5.0 :
            print('')
            print('****************')
            print('** Statistics **')
            print('****************')
            print('')
            print('Correct Replies:  ', cnt_msgok)
            print('Incorrect Replies:', cnt_msgnoneq)
            print('No Replies:       ', cnt_noreply)
            print('')
            last_stat = now

        dt = now - last_tx
        if dt.total_seconds() >= 0.1 :
            last_tx = now
            maybe_write_junk(tty)

            # check if we had a valid message sent the last time
            if sent_msg :
                if not recvd_msg :
                    print('Had sent a message, but got no reply!')
                    cnt_noreply += 1
            sent_msg = False
            recvd_msg = False

            chipid = random.randint(0x40,0x42)
            lastmsg = b'\1'+os.urandom(random.randint(0,15))
            rs485.transmit(chipid, lastmsg)

            if chipid == 0x40 :
                sent_msg = True

        recvmsg = rs485.poll()
        if not recvmsg :
            continue
        if recvmsg != lastmsg :
            print('Messages are not equal. Sent', 
                binascii.hexlify(lastmsg), 'and received',
                binascii.hexlify(recvmsg), '.')
            cnt_msgnoneq += 1
        else :
            print('%s Message %-32s (chip %02x) read back correctly.'%(
                now.strftime('%H:%M:%S.%f'),
                binascii.hexlify(lastmsg).decode('ASCII'), chipid))
            recvd_msg = True
            cnt_msgok += 1

if __name__ == '__main__' :
    main()
