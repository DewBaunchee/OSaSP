/*
Написать программу шифрации всех файлов заданного   
каталога и его подкаталогов. Пользователь задаёт имя каталога. 
Главный процесс открывает каталоги и запускает для каждого файла 
каталога и отдельный процесс шифрации. Каждый процесс выводит 
на экран свой pid, полный путь к файлу, общее число зашифрованных байт. 
Шифрация по алгоритму сложения по модулю 2 бит исходного файла и файла ключа.  
Число одновременно работающих процессов не должно превышать 
N (вводится пользователем). Проверить работу программы для каталога /etc. 
*/

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>

#define REQ_ARG_COUNT 4
#define TEMP_RESULT_FILE "/tmp/tempResult.txt"

const char *progname;
const char *startDir;
FILE *keyFile;
int maxProcessCount;

FILE *tempOut;
time_t duration;

int isCorrectNumber(const char *num)
{
    while (*num != 0)
    {
        if (*num < '0' || *num > '9')
            return 0;
        num++;
    }
    return 1;
}

int checkNumber(const char *num)
{
    if (!isCorrectNumber(num) || atoi(num) == 0)
    {
        fprintf(stderr, "%s: Error. %s is invalid number of process.\n", progname, num);
        return 1;
    }
    return 0;
}

int isDigit(char c)
{
    return c >= '0' && c <= '9';
}

int checkDir(const char *path)
{
    DIR *dir = opendir(path);
    if (dir == 0)
    {
        fprintf(stderr, "%s: Error. Invalid catalog. %s\n", progname, strerror(errno));
        return -1;
    }
    if (closedir(dir) != 0)
    {
        fprintf(stderr, "%s: Error during closing catalog. %s\n", progname, strerror(errno));
        return -1;
    }
    return 0;
}

int checkFile(FILE *file)
{
    if (file == 0)
    {
        fprintf(stderr, "%s: Error. %s.\n", progname, strerror(errno));
        if (errno == 24)
            exit(1);
        return 1;
    }
    return 0;
}

void wrappedCloseFile(FILE *file)
{
    if (fclose(file) != 0)
    {
        fprintf(stderr, "%s: Error during closing file. %s\n", progname, strerror(errno));
    }
}

char *getFullPath(const char *path, const char *name)
{
    char *answer = calloc(strlen(path) + strlen(name) + 2, sizeof(char));
    strcpy(answer, path);
    if (answer[strlen(answer) - 1] != '/')
        strcat(answer, "/");
    strcat(answer, name);
    return answer;
}

void forkEncrypt(const char *plainFile, const char *key, int shm)
{
    switch (fork())
    {
    case -1:
        fprintf(stderr, "%s: Error during fork: %s\n", progname, strerror(errno));
        break;
    case 0:
    {
        int keySize = strlen(key);
        int keyIndex = 0;
        int countOfBytes = 0;
        int *processCount = mmap(0, sizeof(int),
                                 PROT_WRITE | PROT_READ, MAP_SHARED, shm, 0);
        *processCount++;

        FILE *file = fopen(plainFile, "r");
        if (file)
        {
            char plain;
            while ((plain = getc(file)) != EOF)
            {
                char cyphered = plain ^ key[keyIndex];
                keyIndex = (keyIndex + 1) % keySize;
                countOfBytes++;
            }
            wrappedCloseFile(file);

            fprintf(tempOut, "%d %s %d\n", getpid(), plainFile, countOfBytes);
        }
        else
        {
            fprintf(stderr, "%s: Can't open file: %s\n", progname, strerror(errno));
        }
        *processCount--;
        munmap(processCount, sizeof(int));
        exit(EXIT_SUCCESS);
    }
    }
}

void rec(const char *start, const char *key)
{
    int * processCount = 0;
    DIR *dir = opendir(start);
    if (dir == 0)
    {
        printf("%s: Can't open dir: [%s]: %s\n", progname, start, strerror(errno));
        return;
    }

    struct dirent *current;
    int shm = shm_open("shm", O_RDWR, 0777);
    ftruncate(shm, sizeof(int));

    while ((current = readdir(dir)) != NULL)
    {
        if (strcmp(current->d_name, ".") == 0 || strcmp(current->d_name, "..") == 0)
            continue;

        const char *currentPath = getFullPath(start, current->d_name);

        if (current->d_type == DT_DIR)
        {
            rec(currentPath, key);
            continue;
        }

        if (current->d_type == DT_REG)
        {
            if (*processCount >= maxProcessCount)
            {
                wait(NULL);
            }

            forkEncrypt(currentPath, key, shm);
        }
    }

    if (closedir(dir) != 0)
    {
        fprintf(stderr, "%s: Error during closing catalog. %s\n", progname, strerror(errno));
    }

    while (wait(NULL) > 0)
        ;
    close(shm);
    shm_unlink("shm");
}

void solveTask()
{
    fseek(keyFile, 0, SEEK_END);
    int size = ftell(keyFile);
    fseek(keyFile, 0, SEEK_SET);

    if (size < 0)
    {
        fprintf(stderr, "%s: Key file is not a file.\n", progname);
        return;
    }
    if (size == 0)
    {
        fprintf(stderr, "%s: Key file is empty.\n", progname);
        return;
    }

    char *key = calloc(size + 1, sizeof(char));
    for (int i = 0; i < size; i++)
    {
        key[i] = getc(keyFile);
    }
    wrappedCloseFile(keyFile);

    time_t start = time(0);
    rec(startDir, key);
    duration = time(0) - start;

    free(key);
}

int main(int argc, char *argv[])
{
    progname = basename(argv[0]);

    if (argc < REQ_ARG_COUNT)
    {
        fprintf(stderr, "%s: Error. Count of missing argument: %d\n",
                progname, REQ_ARG_COUNT - argc);
        fprintf(stderr, "Command should be: %s <start_catalog> <key_file> <max_count_of_processes>\n", progname);
        return -1;
    }

    startDir = argv[1];
    keyFile = fopen(argv[2], "r");
    char hasErrors = checkDir(startDir) || checkFile(keyFile) || checkNumber(argv[3]);

    if (hasErrors)
        return -1;

    maxProcessCount = atoi(argv[3]);

    tempOut = fopen(TEMP_RESULT_FILE, "w");

    solveTask();

    wrappedCloseFile(tempOut);
    if ((tempOut = fopen(TEMP_RESULT_FILE, "r")))
    {
        while (1)
        {
            char c = getc(tempOut);
            if (feof(tempOut))
                break;
            putc(c, stdout);
        }
    }
    wrappedCloseFile(tempOut);

    printf("Duration: %lds\n", duration);

    return 0;
}