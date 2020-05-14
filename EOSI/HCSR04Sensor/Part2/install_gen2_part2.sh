rm hcsr_platdriver.ko
rm hcsr_platdevice.ko
rm hcsr_test
rm bashtester.sh
dmesg -c
tftp -g -r hcsr_platdriver.ko 192.168.1.1
tftp -g -r hcsr_platdevice.ko  192.168.1.1
tftp -g -r hcsr_test  192.168.1.1
tftp -g -r bashtester.sh 192.168.1.1
rmmod hcsr_platdriver.ko
rmmod hcsr_platdevice.ko
insmod hcsr_platdriver.ko
clear;
sleep 1
insmod hcsr_platdevice.ko
lsmod |grep hcsr
chmod 777 ./hcsr_test
chmod 777 ./bashtester.sh
#./hcsr_test
