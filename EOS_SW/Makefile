ifneq ($(KERNELRELEASE),)
	obj-m := cpu_sw_driver.o
else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
CC ?= gcc
CFLAGS := -Wall -Wextra -O2

default: module app test

module:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

app: app_sw.c program.h program_zbb.h
	$(CC) $(CFLAGS) app_sw.c -o app_sw

# Test ISA jezgra bez kernela (korisno za proveru semantike instrukcija)
test: test_core.c rv32_core.h program.h
	$(CC) $(CFLAGS) test_core.c -o test_core

load: module
	sudo insmod cpu_sw_driver.ko
	sudo chmod 666 /dev/cpu_sw /dev/cpu_sw_mem

unload:
	sudo rmmod cpu_sw_driver

run: app
	./app_sw

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	rm -f app_sw test_core *~

.PHONY: default module app test load unload run clean
endif
