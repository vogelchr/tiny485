#!/usr/bin/python
import time
import serial

SOH = b'\001'
STX = b'\002'
ETX = b'\003'
EOT = b'\004'
ESC = b'\033'

def main() :
    tty = serial.Serial('/dev/ttyUSB0', 9600, timeout=1.0)
    i = 0;
    while True :
        x = SOH+b'@\033'+bytes([i ^ 0x20])+b'@abcde'+EOT
        print(str(x))
        tty.write(x)
        time.sleep(1.0)
        i = (i + 1) % 4;

if __name__ == '__main__' :
    main()