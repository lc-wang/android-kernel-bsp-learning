CC ?= gcc
CFLAGS = -Wall -O2

all: mmap_user

mmap_user: mmap_user.c
	$(CC) $(CFLAGS) -o mmap_user mmap_user.c

clean:
	rm -f mmap_user

