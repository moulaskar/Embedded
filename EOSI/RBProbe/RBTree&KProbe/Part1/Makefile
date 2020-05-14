#CC=gcc
CC = i586-poky-linux-gcc -g -Wall
ARCH = x86
CROSS_COMPILE = i586-poky-linux-
SDKTARGETSYSROOT=/opt/iot-devkit/1.7.2/sysroots/i586-poky-linux
export PATH:=/opt/iot-devkit/1.7.2/sysroots/x86_64-pokysdk-linux/usr/bin:/opt/iot-devkit/1.7.2/sysroots/x86_64-pokysdk-linux/usr/bin/i586-poky-linux:$(PATH)

APP = rbt_tester_probe

#obj-m:= Mydriver.o
#MY_CFLAGS += -Wa, -adhln, -g -DDEBUG
MY_CFLAGS += -O0 -g -static -DDEBUG

#obj-m += RBT_530.o RBprobe.o
obj-m += RBT_530.o

all:
	@echo $$PATH
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(SDKTARGETSYSROOT)/usr/src/kernel M=$(PWD) modules
#	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	$(CC) -o $(APP) main.c -lpthread
debug:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(SDKTARGETSYSROOT)/usr/src/kernel M=$(PWD) modules
	#make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	EXTRA_CFLAGS="$(MY_CFLAGS)"
	$(CC) -o $(APP) main.c -lpthread
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

