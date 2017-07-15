#!/usr/bin/python
import time
import serial
import datetime
import random
import binascii
import os
import argparse

from rs485_proto import RS485_Interface

def nowstr() :
    return datetime.datetime.now().strftime('%H:%M:%S.%f')


def maybe_write_junk(tty) :
    if random.randint(0, 10) == 10 :
        # write random junk
        print('Writing random junk.')
        tty.write(os.urandom(random.randint(0,15)))

def main() :

    parser = argparse.ArgumentParser()
    parser.add_argument('--dev', default='/dev/ttyUSB0', help='Serial Device (def: usb0)')
    parser.add_argument('--baud', default=57600, type=int, help='Baudrate (def: 57k6')
    parser.add_argument('--node', default=0x40, type=int, help='Node ID (def: 0x40)')
    args = parser.parse_args()

    tty = serial.Serial(args.dev, args.baud, timeout=0.05)
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
