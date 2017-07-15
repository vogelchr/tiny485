#!/usr/bin/python
import datetime
import serial
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
    parser.add_argument('--scan', default=False, action='store_true',
        help='Command: Scan for devices.')
    args = parser.parse_args()

    tty = serial.Serial(args.dev, args.baud, timeout=0.05)
    clt = RS485_Interface(tty)

    if args.scan :
        list_of_nodes = [999, 1234]
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

            if msg[0] == b'P'[0] and msg[1] == node and msg[2] == (~node) & 0xff :
                print('Good reply received from node', node, '!');
                list_of_nodes.append(node)
            else :
                print('\nReceived invalid answer:', msg)
        print('Scan finished, found nodes:',
            ', '.join(['%d'%(v) for v in list_of_nodes]))


if __name__ == '__main__' :
    main()