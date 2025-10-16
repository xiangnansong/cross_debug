obj-m += a520_module.o

KERNEL_DIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	make -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	make -C $(KERNEL_DIR) M=$(PWD) clean

install:
	sudo insmod a520_module.ko

uninstall:
	sudo rmmod a520_module

test:
	echo "Testing parameter setting"
	echo "test_parameter" > /sys/class/shm_class/shm_device/param
	cat /sys/class/shm_class/shm_device/param