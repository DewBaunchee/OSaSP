/*
Написать программу подсчета хэша для каждого файла заданного
каталога и его подкаталогов. Пользователь задаёт имя каталога.
Для хэша можно использовать любой алгоритм, дающий приемлемые
результаты. Главный процесс открывает каталоги и запускает
для каждого файла каталога отдельный процесс подсчета хэша.
Каждый процесс выводит на экран свой pid, полный путь к файлу,
общее число просмотренных байт и хэш файла. Число одновременно
работающих процессов не должно превышать N (вводится пользователем).
Проверить работу программы для каталога /etc.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/shm.h>

char *fileName;
int maxChildCounter = 0;
int childsSem;
int parentSem;

#define BUF_SIZE 16 * 1024 * 1024

typedef struct SFileForHash
{
    char path[PATH_MAX];
    struct stat buf;
} * FileForHash;

FileForHash ffh;

void wait_all()
{
    while (wait(NULL) != -1)
        ;
}

void custom_exit(int status)
{
    semctl(childsSem, 0, IPC_RMID);
    semctl(parentSem, 0, IPC_RMID);
    shmdt(ffh);
    exit(status);
}

void interrupt_handler(int signum)
{
    wait_all();
    custom_exit(-1);
}

unsigned long Crc32(unsigned char *buf, unsigned long len)
{
    unsigned long crc_table[256];
    unsigned long crc;
    for (int i = 0; i < 256; i++)
    {
        crc = i;
        for (int j = 0; j < 8; j++)
            crc = crc & 1 ? (crc >> 1) ^ 0xEDB88320UL : crc >> 1;
        crc_table[i] = crc;
    };
    crc = 0xFFFFFFFFUL;
    while (len--)
        crc = crc_table[(crc ^ *buf++) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFUL;
};

void printHash(char *path, struct stat data)
{
    int errors = 0;
    unsigned int currentHash = 0;
    unsigned counterBytes = 0;
    char *buf = calloc(BUF_SIZE, sizeof(char));
    long counter = 0;

    int f = open(path, O_RDONLY);
    if (f == -1)
    {
        // fprintf(stderr, "%d: %s: %s. File: %s\n", getpid(), fileName, strerror(errno), path); tut
        return;
    }

    unsigned long hash = data.st_size ^ data.st_ino ^ data.st_ctim.tv_nsec ^ data.st_atim.tv_nsec;
    while ((counter = read(f, buf, BUF_SIZE)) != 0)
    {
        if (counter == -1)
        {
            fprintf(stderr, "%d: %s: %s. File: %s\n", getpid(), fileName, strerror(errno), path);
            errors++;
            break;
        }

        hash = Crc32(buf, counter);
        counterBytes += counter;
    }

    if (counterBytes != data.st_size)
    {
        fprintf(stderr, "%d: %s: %s. File %s\n", getpid(), fileName, strerror(errno), path);
        errors++;
    }

    free(buf);

    if (!errors)
        printf("%d: %s %u %lx\n", getpid(), path, counterBytes, hash);

    if (close(f) == -1)
    {
        fprintf(stderr, "%d: %s: %s. File: %s\n", getpid(), fileName, strerror(errno), path);
    }
}

void semaphore_op(int semaphore, int val)
{
    struct sembuf op = {0, val, 0};
    while (semop(semaphore, &op, 1))
            fprintf(stderr, "%s: pid %d: Error while adding %d to semaphore: %s\n", fileName, getpid(), val, strerror(errno));
}

void processes(char *currentPath)
{
    DIR *currentDir;
    struct dirent *structDir;

    if ((currentDir = opendir(currentPath)) == NULL)
    {
        // fprintf(stderr, "%d: %s: %s. File: %s\n", getpid(), fileName, strerror(errno), currentPath); tut tri
        errno = 0;
        return;
    }

    char *file = calloc(strlen(currentPath) + NAME_MAX + 2, sizeof(char));
    if (file == NULL)
    {
        fprintf(stderr, "%d: %s: %s.", getpid(), fileName, strerror(errno));
        return;
    }
    errno = 0;

    while (structDir = readdir(currentDir))
    {
        if (strcmp(".", structDir->d_name) && strcmp("..", structDir->d_name))
        {
            strcpy(file, currentPath);
            if (file[strlen(file) - 1] != '/')
                strcat(file, "/");
            strcat(file, structDir->d_name);

            struct stat buf;

            if (lstat(file, &buf) == -1)
            {
                fprintf(stderr, "%d: %s: %s. File: %s\n", getpid(), fileName, strerror(errno), currentPath);
                return;
            }

            if (structDir->d_type == DT_DIR)
            {
                processes(file);
                continue;
            }

            if (structDir->d_type == DT_REG)
            {
                semaphore_op(childsSem, 0);
                semaphore_op(childsSem, 1);
                strcpy(ffh->path, file);
                ffh->buf = buf;
                semaphore_op(parentSem, 1);
            }
        }
    }

    if (errno != 0)
    {
        // fprintf(stderr, "%d: %s: %s. File: %s\n", getpid(), fileName, strerror(errno), currentPath); tut mnoga
        errno = 0;
        return;
    }

    if (closedir(currentDir) == -1)
    {
        fprintf(stderr, "%d: %s: %s. File: %s\n", getpid(), fileName, strerror(errno), currentPath);
        errno = 0;
        return;
    }

    free(file);
}

int main(int argc, char **argv)
{
    fileName = basename(argv[0]);

    if (argc < 3)
    {
        fprintf(stderr, "%d: %s: Too few arguments\n", getpid(), fileName);
        return -1;
    }

    maxChildCounter = atoi(argv[2]) - 1;

    if (maxChildCounter < 1)
    {
        fprintf(stderr, "%d: %s: Second argument must be number\n", getpid(), fileName);
        return -1;
    }

    char *result;
    if ((result = realpath(argv[1], NULL)) == NULL)
    {
        fprintf(stderr, "%d: %s: %s. File: %s.\n", getpid(), fileName, strerror(errno), argv[1]);
        return -1;
    }

    signal(SIGINT, &interrupt_handler);
    signal(SIGCHLD, SIG_IGN);
    childsSem = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (childsSem == -1)
    {
        fprintf(stderr, "%s: pid %d: Error with creating semaphore: %s\n", fileName, getpid(), strerror(errno));
        custom_exit(-1);
    }
    parentSem = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (parentSem == -1)
    {
        fprintf(stderr, "%s: pid %d: Error with creating semaphore: %s\n", fileName, getpid(), strerror(errno));
        custom_exit(-1);
    }

    int id = shmget(IPC_PRIVATE, sizeof(struct SFileForHash), IPC_CREAT | 0644);

    for (int i = 0; i < maxChildCounter; i++)
    {
        switch (fork())
        {
        case -1:
            fprintf(stderr, "%d: %s: %s.\n", getpid(), fileName, strerror(errno));
            errno = 0;
            custom_exit(-1);
            break;
        case 0:
            ffh = (FileForHash)shmat(id, NULL, 0);

            while (1)
            {
                semaphore_op(parentSem, -1);
                if (ffh->path[0] == 0)
                    break;

                char *path = calloc(strlen(ffh->path) + 1, sizeof(char));
                struct stat buf = ffh->buf;
                strcpy(path, ffh->path);
                semaphore_op(childsSem, -1);

                printHash(path, buf);
            }

            shmdt(ffh);
            exit(0);
        }
    }

    ffh = (FileForHash)shmat(id, NULL, 0);
    processes(result);
    ffh->path[0] = 0;
    semaphore_op(parentSem, maxChildCounter);

    wait_all();
    custom_exit(0);
}
