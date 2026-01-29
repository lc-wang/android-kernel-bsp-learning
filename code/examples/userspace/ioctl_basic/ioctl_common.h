#ifndef IOCTL_COMMON_H
#define IOCTL_COMMON_H

#include <linux/ioctl.h>

#define MY_IOCTL_MAGIC 'M'

#define MY_IOCTL_RESET   _IO(MY_IOCTL_MAGIC, 0)
#define MY_IOCTL_SET_VAL _IOW(MY_IOCTL_MAGIC, 1, int)
#define MY_IOCTL_GET_VAL _IOR(MY_IOCTL_MAGIC, 2, int)

#endif

