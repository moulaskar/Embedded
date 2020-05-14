#CC=gcc
CC = i586-poky-linux-gcc -g -Wall
#CC = i586-poky-linux-gcc -g 
ARCH = x86
CROSS_COMPILE = i586-poky-linux-
SDKTARGETSYSROOT=/opt/iot-devkit/1.7.2/sysroots/i586-poky-linux
export PATH:=/opt/iot-devkit/1.7.2/sysroots/x86_64-pokysdk-linux/usr/bin:/opt/iot-devkit/1.7.2/sysroots/x86_64-pokysdk-linux/usr/bin/i586-poky-linux:$(PATH)

APP = hcsr_test

#obj-m:= Mydriver.o
#MY_CFLAGS += -Wa, -adhln, -g -DDEBUG
MY_CFLAGS += -O0 -g -static -DDEBUG

obj-m += hcsr_platdriver.o hcsr_platdevice.o
#obj-m += hcsr_platdriver.o
#obj-m += hcsr_platdevice.o

all:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(SDKTARGETSYSROOT)/usr/src/kernel M=$(PWD) modules
#	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	$(CC) -o $(APP) main.c 
debug:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(SDKTARGETSYSROOT)/usr/src/kernel M=$(PWD) modules
	#make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	EXTRA_CFLAGS="$(MY_CFLAGS)"
	$(CC) -o $(APP) main.c
#clean:
#	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean



#all:
#        make ARCH=x86 CROSS_COMPILE=i586-poky-linux- -C $(SDKTARGETSYSROOT)/usr/src/kernel M=$(PWD) modules
#        $(CC) -o $(APP) main.c --sysroot=$(SDKTARGETSYSROOT)

#clean:
clean:
	rm -f *.ko
	rm -f *.o
	rm -f Module.symvers
	rm -f modules.order
	rm -f *.mod.c
	rm -rf .tmp_versions
	rm -f *.mod.c
	rm -f *.mod.o
	rm -f \.*.cmd
	rm -f Module.markers
	rm -f $(APP) 

