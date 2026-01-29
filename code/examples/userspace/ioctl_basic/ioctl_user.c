#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "ioctl_common.h"

int main(void)
{
	int fd;
	int val = 123;

	fd = open("/dev/mychardev", O_RDWR);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	printf("ioctl: RESET\n");
	ioctl(fd, MY_IOCTL_RESET);

	printf("ioctl: SET_VAL = %d\n", val);
	ioctl(fd, MY_IOCTL_SET_VAL, &val);

	val = 0;
	printf("ioctl: GET_VAL\n");
	ioctl(fd, MY_IOCTL_GET_VAL, &val);
	printf("value from kernel = %d\n", val);

	close(fd);
	return 0;
}

