# SPDX-License-Identifier: GPL-2.0

# Kernel build system integration
obj-m := spinlock_example.o mutex_example.o completion_example.o

# Default target
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

# Clean target
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
