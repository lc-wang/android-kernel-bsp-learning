CC ?= gcc
CFLAGS = -Wall -O2

all: ioctl_user

ioctl_user: ioctl_user.c
	$(CC) $(CFLAGS) -o ioctl_user ioctl_user.c

clean:
	rm -f ioctl_user

