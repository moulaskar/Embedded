rm hcsr.ko
rm hcsr_test
dmesg -c
tftp -g -r hcsr.ko 192.168.1.1
tftp -g -r hcsr_test 192.168.1.1
rmmod hcsr.ko
clear;
sleep 1
insmod hcsr.ko max_device=2
lsmod |grep hcsr
chmod 777 ./hcsr_test
#./hcsr_test
