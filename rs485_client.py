#!/usr/bin/python
import datetime
import serial
import struct
import time
import sys
from rs485_proto import RS485_Interface

class Timeout :
    def __init__(self, to_sec=1.0) :
        self.to = to_sec
        self.reset()

    def reset(self) :
        self.last_reset = datetime.datetime.now()

    def check(self) :
        now = datetime.datetime.now()
        dt = now - self.last_reset
        if dt.total_seconds() >= self.to :
            self.reset()
            return True
        return False

def main() :
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--dev', default='/dev/ttyUSB0',
        help='Serial Device (def: /dev/ttyUSB0)')
    parser.add_argument('--baud', default=57600, type=int,
        help='Baudrate (def: 57600)')
    parser.add_argument('--node', default=0x40, type=int,
        help='Node ID (def: 64)')

    ###
    # commands
    ###
    parser.add_argument('--scan', default=False, action='store_true',
        help='Command: Scan for devices.')
    parser.add_argument('--set-node', default=None, type=int,
        help='Command: Set new node address.')
    parser.add_argument('--get-servo', default=False,
        action='store_true', help='Command: Get servo configuration.')
    parser.add_argument('--set-servo',
        metavar="s1[,s2[,period]]",
        default=None, type=str,
        help='Command: Set servo output(s), times in usec.')


    args = parser.parse_args()
    tty = serial.Serial(args.dev, args.baud, timeout=0.01)
    clt = RS485_Interface(tty)

    if args.scan :
        list_of_nodes = [ ]
        node = None
        to = Timeout(0.02)
        while True :
            if node == None or to.check() :
                if node is None :
                    node = 0
                else :
                    if node == 0xff :
                        break
                    node += 1
                print('Querying node %03d.'%(node), end='\r')
                clt.transmit(node, b'P' + bytes([node, (~node) & 0xff ]))

            msg = clt.poll()
            if msg == None :
                continue

            if len(msg) != 3 :
                print('Bad length message received!', msg)
                continue

            if msg[0] == b'P'[0] and msg[1] == node and msg[2] == (~node) & 0xff :
                print('Good reply received from node', node, '!');
                list_of_nodes.append(node)
            else :
                print('\nReceived invalid answer:', msg)
        print('Scan finished, found nodes:',
            ', '.join(['%d'%(v) for v in list_of_nodes]))

    if args.set_servo :
        arr = args.set_servo.split(',')
        if not len(arr) or len(arr) > 3 :
            print('You may only specify 1..3 values!')
            sys.exit(1);

        cmd = b'S'
        s1 = int(arr[0])
        cmd += struct.pack('<H', s1)
        print('Set servo output 1 to %d us'%s1, end='')
        if len(arr) >= 2 :
            s2 = int(arr[1])
            print(', output 2 to %d us'%s2, end='')
            cmd += struct.pack('<H', s2)
        if len(arr) >= 3 :
            per = int(arr[2])
            print(', period to %d us'%per, end='')
            cmd += struct.pack('<H', int(per-1))
        print('.')
        clt.transmit(args.node, cmd)
        time.sleep(0.5)
        print('Reading back.')

    if args.get_servo or args.set_servo :
        to = Timeout(1.0)
        while True :
            clt.transmit(args.node, b'Q')
            msg = clt.poll()
            if to.check() :
                print('Timeout waiting for answer!')
                sys.exit(1);

            if msg == None :
                continue

            if len(msg) != 8 :
                print('Bad answer (length != 8) received.', msg)
                continue

            rxnode, cmdid, s1, s2, per = struct.unpack('<BBHHH', msg)
            if rxnode != args.node and cmdid != b'Q'[0] :
                print('Bad answer received?', msg)
                continue
            print('Servo configuration:')
            print('  Servo 1 Output: %6d usec'%s1)
            print('  Servo 2 Output: %6d usec'%s2)
            print('  Pulse period:   %6d usec'%(per+1))
            break



if __name__ == '__main__' :
    main()