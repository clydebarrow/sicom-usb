#
#	Makefile for the Sicom firmware
#

.SUFFIXES:
.SUFFIXES:	.c .obj .as .yaml

PWD = $(shell pwd)
DOCKER = docker run --rm -it --platform linux/386 -v$(PWD):/home/user/project clydeps/c51:latest
C	= $(DOCKER) c51
AS	= $(DOCKER) c51
FIRMWARE =  $(DOCKER) firmware
ASFLAGS	= -P
CHIP=c8051f320
USBCONFIG=utilities/usbconfig.py
DEBUG	= -UDEBUG -ULOGGING -UUSB_LOGGING
CFLAGS	= -q --chip=$(CHIP) $(DBL) $(ROM) --opt=all -g --asmlist -I. $(DEBUG) --runtime=+keep
NOOPT_CFLAGS	= -q --chip=$(CHIP) $(DBL) $(ROM) --opt=none


VERS := 1.00


LD	= $(BIN)/link
RMAKE	= $(MAKE) -f Makefile

%.c:	%.yaml
	$(USBCONFIG)  $<  $@
%.obj:	%.as
	$(C) $(CFLAGS) $(ASFLAGS) -c $<
%.obj:	%.c
	$(C) $(CFLAGS) -c $<
#%.p1:	$(COM)/%.c
#	$(C) $(CFLAGS) --pass1 $<
#%.p1:	%.c
#	$(C) $(CFLAGS) --pass1 $<


SRCS	 = \
	   main.c sicom.yaml bootlib.as crc.c crc.h htusb.h

OBJS	 = \
		main.obj sicom.obj crc.obj

all:	  sicom.hex

sicom.c: sicom.yaml

sicom.hex:	$(OBJS) Makefile
	$(C) --rom=default,-0-fff --codeoffset=1000h $(CFLAGS) -osicom.hex -msicom.map $(OBJS) bootlib.as
	$(FIRMWARE) -osicom.fmw -c1000 -v$(VERS) -p320 sicom.hex
#	$(C) --chip=$(CHIP) -osicom.hex -msicom.map $(OBJS)

srclist:
	 @echo $(SRCS)

objlist:
	 @echo $(OBJS)

rmobjs:
		@rm -f $(OBJS) *.map *.sym *.sdb *.lst *.hex *.omf *.rlf startup.as *.obj *.p1 hexfile.log

clean:		rmobjs
		@rm -f sicom.c

clobber:	clean
	@rm -f sicom.fmw
		
main.obj:	main.c htusb.h Makefile 
