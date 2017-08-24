# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
CONFIG_MODULE_SIG=n
ifneq ($(KERNELRELEASE),)
	#obj-m := parrot_driver.o
	#obj-m := examplchar.o
	#obj-m := perfTool.o
	#obj-m := mmdc.o
	obj-m := perf-pcie.o
	#obj-m := ofcd.o
# Otherwise we were called directly from the command
# line; invoke the kernel build system.
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	rm -f *.o *.ko *.mod.c *.order *.symvers
endif
insmod:
	sudo insmod perf-pcie.ko
rmmod:
	sudo rmmod perf-pcie
