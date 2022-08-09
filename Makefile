# make ARCH=arm modname=demoA
# make ARCH=x86 modname=demoA
ifeq ($(arch),arm)
	# 指定开发板的内核源码的路径
	KERNELDIR := /home/ubuntu/linux-5.10.61
	CROSS_COMPILE ?= arm-linux-gnueabihf-
else
	# 指定ubuntu的内核源码的路径
	KERNELDIR := /lib/modules/$(shell uname -r)/build
	CROSS_COMPILE ?=
endif

modname ?=
# 执行pwd命令获取当前的路径，赋值给变量PWD
PWD := $(shell pwd)
CC := $(CROSS_COMPILE)gcc

all:
	make -C $(KERNELDIR)  M=$(PWD)   modules
	$(CC) test.c -o test
clean:
	make -C $(KERNELDIR)  M=$(PWD)   clean
	rm test

install:
	cp *.ko ~/nfs/rootfs
	cp test ~/nfs/rootfs

help:
	echo "make ARCH=arm or x86 modname=drivers file name"

obj-m := $(modname).o