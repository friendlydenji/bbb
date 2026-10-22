#include "ioctl.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>

int ioctl_set_msg(int fd, const char *msg)
{
	int retval;

	retval = ioctl(fd, IOCTL_SET_MSG, msg);

	if (retval < 0) {
		printf("ioctl_set_msg failed: %d\n", retval);
	}
	
	printf("ioctl_set_msg succeed\n");

	return retval;
}

int ioctl_get_msg(int fd, char *msg)
{
	int retval;
	
	retval = ioctl(fd, IOCTL_GET_MSG, msg);

	if (retval < 0) {
		printf("ioctl_get_msg failed: %d\n", retval);
	}
	
	printf("ioctl_get_msg succeed\n");

	return retval;
}

int ioctl_get_nth_byte(int fd, long pos, char *ch)
{
	int retval;

	retval = ioctl(fd, IOCTL_GET_NTH_BYTE, pos); 

	if (retval < 0) {
		printf("ioctl_get_nth_byte failed: %d\n", retval);
	}
	
	*ch = (char)retval;

	return retval;
}

int main(void)
{
	int fd;

	const char *set_msg = "12345678\n";
	char msg[100];

	fd = open(DEVICE_PATH, O_RDWR);

	if (fd < 0) {
		printf("open device file %s failed: %d", DEVICE_PATH, fd);
		exit(EXIT_FAILURE);
	}

	int retval;
	retval = ioctl_get_msg(fd, msg);
	if (retval) {
		goto err;
	}
	printf("message read %s", msg);

	retval = ioctl_set_msg(fd, set_msg);
	if (retval) {
		goto err;
	}

	char seven;
	retval = ioctl_get_nth_byte(fd, 7, &seven);
	if (retval) {
		goto err;
	}
	printf("char got %c", seven);
	close(fd);
	return 0;
err:
	close(fd);
	exit(EXIT_FAILURE);
}

