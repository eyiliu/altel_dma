obj-m := axidmachar.o
ccflags-y := -std=gnu99 -Wno-declaration-after-statement

SRC := $(shell pwd)

KERNEL_SRC := /usr/src/kernels/4.19.0-xilinx-v2019.2

all:
	make -C  $(KERNEL_SRC) M=$(SRC)

modules_install:
	make -C  $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c *.o.d
	rm -f Module.markers Module.symvers modules.order
	rm -rf .tmp_versions Modules.symvers
