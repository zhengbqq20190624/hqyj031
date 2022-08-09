#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include "mypwm.h"

int main(int argc, const char *argv[])
{
    int status;
    int fd;
    int a;
    char buf[128] = "i am fine..........\n";

    if ((fd = open("/dev/platform_cdev", O_RDWR)) == -1)
    {
        perror("open error");
        exit(EXIT_FAILURE);
    }
    write(fd, buf, sizeof(buf));


    printf("请输入0~6 >>");

    while (1)
    {
        scanf("%d", &status);
        while (getchar() != 10)
            ;

        printf("status = %d\n", status);
        switch (status)
        {
        case 1:
            ioctl(fd, FAN_ON);
            break;
        case 2:
            ioctl(fd, FAN_OFF);
            break;
        case 3:
            ioctl(fd, BEE_ON);
            break;
        case 4:
            ioctl(fd, BEE_OFF);
            break;
        case 5:
            ioctl(fd, MOR_ON);
            break;
        case 6:
            ioctl(fd, MOR_OFF);
            break;
        }
    }
    close(fd);
    return 0;
}