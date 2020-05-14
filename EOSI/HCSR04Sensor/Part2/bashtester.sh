#!/bin/bash

#Setting trigger and echo pins for device 1
	echo -n "Enter trigger pin for device 1 -> "
	read trigger
	echo "$trigger" > /sys/class/HCSR/HCSR_1/trigger
	echo -n "Enter echo pin  for device 1 -> "
	read echo_pin
	echo "$echo_pin" > /sys/class/HCSR/HCSR_1/echo
	
#Setting trigger and echo pins for device 2
	echo -n "Enter trigger pin for device 2 -> "
	read trigger
	echo "$trigger" > /sys/class/HCSR/HCSR_2/trigger
	echo -n "Enter echo pin  for device 2 -> "
	read echo_pin
	echo "$echo_pin" > /sys/class/HCSR/HCSR_2/echo
	
	
#Case 1(device 1): Periodic sampling_period, number_samples=9
	echo "Case 2(device 1): Periodic sampling_period(sampling_period=200, number_samples=9, enable=1(later 0))"
	echo "200" > /sys/class/HCSR/HCSR_1/sampling_period
	echo "9" > /sys/class/HCSR/HCSR_1/number_samples
	echo "1" > /sys/class/HCSR/HCSR_1/enable
        sleep 3
	echo "0" > /sys/class/HCSR/HCSR_1/enable
#Reading distance value device 1
	echo "Last value of distance measured device 1"
	cat /sys/class/HCSR/HCSR_1/distance

#Case 1(device 2): Periodic sampling_period, number_samples=9
	echo "Case 2(device 2): Periodic sampling_period(sampling_period=200, number_samples=9, enable=1(later 0))"
	echo "200" > /sys/class/HCSR/HCSR_2/sampling_period
	echo "9" > /sys/class/HCSR/HCSR_2/number_samples
	echo "1" > /sys/class/HCSR/HCSR_2/enable
        sleep 3
	echo "0" > /sys/class/HCSR/HCSR_2/enable
#Reading distance value device 2
	echo "Last value of distance measured device 2:"
	cat /sys/class/HCSR/HCSR_2/distance



#Case 2(device 1): Periodic sampling_period, number_samples=5
	echo "Case 2(device 1): Periodic sampling_period(sampling_period=200, number_samples=5, enable=1(later 0))"
	echo "200" > /sys/class/HCSR/HCSR_1/sampling_period
	echo "5" > /sys/class/HCSR/HCSR_1/number_samples
	echo "1" > /sys/class/HCSR/HCSR_1/enable
        sleep 3
	echo "0" > /sys/class/HCSR/HCSR_1/enable
#Reading distance value device 1
	echo "Last value of distance measured device 1:"
	cat /sys/class/HCSR/HCSR_1/distance

#Case 2(device 2): Periodic sampling_period, number_samples=5
	echo "Case 2(device 2): Periodic sampling_period(sampling_period=200, number_samples=5, enable=1(later 0))"
	echo "200" > /sys/class/HCSR/HCSR_2/sampling_period
	echo "5" > /sys/class/HCSR/HCSR_2/number_samples
	echo "1" > /sys/class/HCSR/HCSR_2/enable
        sleep 3
	echo "0" > /sys/class/HCSR/HCSR_2/enable
#Reading distance value device 2
	echo "Last value of distance measured device 2:"
	cat /sys/class/HCSR/HCSR_2/distance
