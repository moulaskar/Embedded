KDIR:=/opt/iot-devkit/1.7.2/sysroots/i586-poky-linux/usr/src/kernel
CC = i586-poky-linux-gcc
CROSS_COMPILE = i586-poky-linux-
export PATH:=/opt/iot-devkit/1.7.2/sysroots/x86_64-pokysdk-linux/usr/bin:/opt/iot-devkit/1.7.2/sysroots/x86_64-pokysdk-linux/usr/bin/i586-poky-linux:$(PATH)
PWD:= $(shell pwd)

ARCH = x86
SROOT=/opt/iot-devkit/1.7.2/sysroots/i586-poky-linux
EXTRA_CFLAGS += -Wall

LDLIBS = -L$(SROOT)/usr/lib
CCFLAGS = -I$(SROOT)/usr/include/libnl3

APP = 
EXAMPLE = genl_app

obj-m:= interface.o

.PHONY:all
all: genl_app hcsr.ko

hcsr.ko:
	make ARCH=x86 CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) modules

genl_app:
	$(CC) -Wall -o $(EXAMPLE) genl_ex.c $(CCFLAGS) -lnl-genl-3 -lnl-3 -lpthread


.PHONY:clean
clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f *.o $(EXAMPLE) $(APP) 

