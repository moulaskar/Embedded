ARCH=x86
LOCALVERSION= 
INSTALL_MOD_PATH=../galileo-install
CROSS_COMPILE=i586-poky-linux-
export PATH=/opt/iot-devkit/1.7.2/sysroots/x86_64-pokysdk-linux/usr/bin/i586-poky-linux:$PATH
make headers_check
make headers_install
#make scripts
make ARCH=x86 CROSS_COMPILE=i586-poky-linux-
ls -l arch/x86/boot/ | grep bzImage
cp arch/x86/boot/bzImage /tftpboot/
make
cp dump_stack_tester /tftpboot/
