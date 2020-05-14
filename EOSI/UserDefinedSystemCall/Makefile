CC = i586-poky-linux-gcc -g -Wall
ARCH = x86
CROSS_COMPILE = i586-poky-linux-
SDKTARGETSYSROOT=/opt/iot-devkit/1.7.2/sysroots/i586-poky-linux
export PATH:=/opt/iot-devkit/1.7.2/sysroots/x86_64-pokysdk-linux/usr/bin:/opt/iot-devkit/1.7.2/sysroots/x86_64-pokysdk-linux/usr/bin/i586-poky-linux:$(PATH)

APP = dump_stack_tester
MY_CFLAGS += -O0 -g -static -DDEBUG

obj-m += main.o 

all:
	$(CC) -o $(APP) main.c -lpthread
clean:
	rm -f $(APP) 

