DEVICE_CC = attiny2313
DEVICE_DUDE = t2313

PROGRAMMER_DUDE = -Pusb -c dragon_isp
# PROGRAMMER_DUDE = -P/dev/ttyUSB0 -c stk500v1 -b 57600

# orig: E:FF, H:DF, L:64
# CLKDIV8 not set, --> 8 MHz
FUSE_L=0xe4
FUSE_H=0xdf
FUSE_E=0xff

AVRDUDE=avrdude
OBJCOPY=avr-objcopy
OBJDUMP=avr-objdump
CC=avr-gcc
LD=avr-gcc
SIZE=avr-size

LDFLAGS=-Wall -mmcu=$(DEVICE_CC)
CPPFLAGS=
CFLAGS=-mmcu=$(DEVICE_CC) -Os -Wall -g3 -ggdb -DF_CPU=8000000UL

MYNAME=avr-stepper-iface

OBJS=$(MYNAME).o rs485.o tiny485_syscfg.o stepper.o

all : $(MYNAME).hex $(MYNAME).lst

$(MYNAME).bin : $(OBJS)

%.hex : %.bin
	$(OBJCOPY) -j .text -j .data -O ihex $^ $@ || (rm -f $@ ; false )

%.lst : %.bin
	$(OBJDUMP) -S $^ >$@ || (rm -f $@ ; false )

%.bin : %.o
	$(LD) $(LDFLAGS) -o $@ $^
	$(SIZE) -C --mcu=$(DEVICE_CC) $@

include $(OBJS:.o=.d)

%.d : %.c
	$(CC) -o $@ -MM $^

.PHONY : clean burn fuse
burn : $(MYNAME).hex
	$(AVRDUDE) $(PROGRAMMER_DUDE) -p $(DEVICE_DUDE) -U flash:w:$^
fuse :
	$(AVRDUDE) $(PROGRAMMER_DUDE) -p $(DEVICE_DUDE) \
		-U lfuse:w:$(FUSE_L):m -U hfuse:w:$(FUSE_H):m \
		-U efuse:w:$(FUSE_E):m
clean :
	rm -f *.bak *~ *.bin *.hex *.lst *.o *.d
