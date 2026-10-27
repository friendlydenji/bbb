#define _GNU_SOURCE
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>

int main(void)
{
    int fd = syscall(SYS_openat, AT_FDCWD, "/init", O_RDONLY, 0);
    if (fd < 0)
        return 1;
    close(fd);
    return 0;
}