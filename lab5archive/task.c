#include <stdio.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define COMMON_FILE "common"
#define BUFFER_SIZE 1024

int getTime()
{
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    return spec.tv_nsec / 1.0e6;
}

int main()
{
    struct sembuf waitForWrote = {0, -1, 0};
    struct sembuf waitForRead = {0, 0, 0};
    struct sembuf writeEnd = {0, 2, 0};
    struct sembuf readEnd = {0, -1, 0};
    int sem = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);

    if (fork())
    {
        // parent
        int fd = open(COMMON_FILE, O_RDWR | O_CREAT);
        char *buf = calloc(BUFFER_SIZE, sizeof(char));

        for (int i = 0; i < 100; i++)
        {
            printf("Waiting for wrote.\n");
            if (semop(sem, &waitForWrote, 1))
            {
                printf("Error in parent: %s\n", strerror(errno));
                break;
            }
            int count = read(fd, buf, BUFFER_SIZE);

            printf("Reading: %d ", getpid());
            for (int c = 0; c < count; c++)
            {
                putchar(buf[c]);
            }

            printf("Read end\n");
            if (semop(sem, &readEnd, 1))
            {
                printf("Error in parent: %s\n", strerror(errno));
                break;
            }
        }

        close(fd);
        free(buf);
    }
    else
    {
        // child
        int fd = open(COMMON_FILE, O_RDWR | O_CREAT);
        char *buf = calloc(BUFFER_SIZE, sizeof(char));

        for (int i = 0; i < 100; i++)
        {
            sleep(1);
            printf("Writing\n");
            sprintf(buf, "%d %d %d\n", i + 1, getpid(), getTime());

            write(fd, buf, strlen(buf));

            printf("Write end\n");
            if (semop(sem, &writeEnd, 1))
            {
                printf("Error in parent: %s\n", strerror(errno));
                break;
            }
            printf("Waiting for read\n");
            if (semop(sem, &waitForRead, 1))
            {
                printf("Error in parent: %s\n", strerror(errno));
                break;
            }
        }

        close(fd);
        free(buf);
    }

    return 0;
}