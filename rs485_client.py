#!/usr/bin/python
import serial
from rs485_proto import RS485_Interface


def main() :
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--dev', default='/dev/ttyUSB0', help='Serial Device (def: usb0)')
    parser.add_argument('--baud', default=57600, type=int, help='Baudrate (def: 57k6')
    parser.add_argument('--node', default=0x40, type=int, help='Node ID (def: 0x40)')
    tty = serial.Serial(args.dev, args.baud, timeout=0.05)
    rs485 = RS485_Interface(tty)

if __name__ == '__main__' :
    main()