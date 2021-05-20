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
#include <fcntl.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

#define REQ_ARG_COUNT 4
#define TEMP_RESULT_FILE "/tmp/tempResult.txt"
#define CYPHER_FOLDER "/tmp/cyphered"
#define BUFFER_SIZE 16 * 1024 * 1024

//#define USE_TEMP_FILE

const char *progname;
char *key;
char *startDir;
FILE *keyFile;
int maxThreadCount;

FILE *tempOut;
pthread_t *THREADS;
enum SLOT_STATUS *SLOT_STATUSES;

typedef struct
{
    int id;
    enum SLOT_STATUS *selfStatus;
    char *pfile;
    char *cfile;
} ThreadParams;

enum SLOT_STATUS
{
    SS_FINISHED,
    SS_IN_PROGRESS
};

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
        fprintf(stderr, "%s: tid: %ld: Error. %s is invalid number of process.\n", progname, pthread_self(), num);
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
        fprintf(stderr, "%s: tid: %ld: Error. Invalid catalog. %s\n", progname, pthread_self(), strerror(errno));
        return -1;
    }
    if (closedir(dir) != 0)
    {
        fprintf(stderr, "%s: tid: %ld: Error during closing catalog. %s\n", progname, pthread_self(), strerror(errno));
        return -1;
    }
    return 0;
}

int checkFile(FILE *file)
{
    if (file == 0)
    {
        fprintf(stderr, "%s: tid: %ld: Error. %s.\n", progname, pthread_self(), strerror(errno));
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
        fprintf(stderr, "%s: tid: %ld: Error during closing file. %s\n", progname, pthread_self(), strerror(errno));
    }
}

void encrypt(const char *plainFile, const char *cypheredFile, const char *key)
{
    int keySize = strlen(key);
    int keyIndex = 0;
    int countOfBytes = 0;

    int pfile = open(plainFile, O_RDONLY);
    int cfile = open(cypheredFile, O_WRONLY | O_CREAT);
    if (pfile && cfile)
    {
        char *buffer = calloc(BUFFER_SIZE, sizeof(char));
        int count;

        while ((count = read(pfile, buffer, BUFFER_SIZE)) > 0)
        {
            char *cyphered = calloc(count, sizeof(char));
            for (int i = 0; i < count; i++)
            {
                cyphered[i] = buffer[i] ^ key[keyIndex];
                keyIndex = (keyIndex + 1) % keySize;
            }
            countOfBytes += count;

            if (write(cfile, cyphered, count) != count)
            {
                fprintf(stderr, "%s: tid: %ld: Error while writing in file %s: %s\n", progname, pthread_self(), cypheredFile, strerror(errno));
                return;
            }
        }

        if (count == -1)
        {
            fprintf(stderr, "%s: tid: %ld: Error in file %s: %s\n", progname, pthread_self(), plainFile, strerror(errno));
        }

        int hasErrors = 0;
        int status = close(pfile);
        if (status == -1)
        {
            hasErrors = 1;
            fprintf(stderr, "%s: tid: %ld: Error while closing file %s: %s\n", progname, pthread_self(), plainFile, strerror(errno));
        }

        status = close(cfile);
        if (status == -1)
        {
            hasErrors = 1;
            fprintf(stderr, "%s: tid: %ld: Error while closing file %s: %s\n", progname, pthread_self(), cypheredFile, strerror(errno));
        }
        
        free(buffer);
        if (hasErrors) 
            return;

#ifdef USE_TEMP_FILE
        fprintf(tempOut, "%ld %s %d\n", pthread_self(), plainFile, countOfBytes);
#else
        printf("%ld %s %d\n", pthread_self(), plainFile, countOfBytes);
#endif
    }
    else
    {
        fprintf(stderr, "%s: tid: %ld: Can't open file: %s\n", progname, pthread_self(), strerror(errno));
    }
}

void *runnable(void *params)
{
    ThreadParams *threadParams = (ThreadParams *)params;

    *(threadParams->selfStatus) = SS_IN_PROGRESS;
    encrypt(threadParams->pfile, threadParams->cfile, key);

    free(threadParams->pfile);
    free(threadParams->cfile);
    *(threadParams->selfStatus) = SS_FINISHED;
    free(threadParams);
    return 0;
}

int getFreeSlotIndex(int count)
{
    int i = 0;
    while (SLOT_STATUSES[i] == SS_IN_PROGRESS)
        i = (i + 1) % count;
    return i;
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

void rec(const char *start)
{
    DIR *dir = opendir(start);
    if (dir == 0)
    {
        fprintf(stderr, "%s: tid: %ld: Can't open dir: [%s]: %s\n", progname, pthread_self(), start, strerror(errno));
        return;
    }
    mkdir(getFullPath(CYPHER_FOLDER, start + 1), 0777);

    struct dirent *current;

    while ((current = readdir(dir)) != NULL)
    {
        if (strcmp(current->d_name, ".") == 0 || strcmp(current->d_name, "..") == 0)
            continue;

        const char *currentPath = getFullPath(start, current->d_name);

        if (current->d_type == DT_DIR)
        {
            rec(currentPath);
            continue;
        }

        if (current->d_type == DT_REG)
        {
            pthread_t tid;
            int slotIndex = getFreeSlotIndex(maxThreadCount);

            if (pthread_join(THREADS[slotIndex], NULL) < 0)
            {
                fprintf(stderr, "%s: Error during pthread_join: %s\n", progname, strerror(errno));
                return;
            }

            ThreadParams *threadParams = calloc(1, sizeof(ThreadParams));
            threadParams->id = slotIndex;
            threadParams->selfStatus = &(SLOT_STATUSES[slotIndex]);
            threadParams->pfile = calloc(strlen(currentPath) + 1, sizeof(char));
            strcpy(threadParams->pfile, currentPath);
            threadParams->cfile = getFullPath(CYPHER_FOLDER, currentPath + 1);

            SLOT_STATUSES[slotIndex] = SS_IN_PROGRESS;
            if (pthread_create(&tid, 0, &runnable, threadParams))
            {
                fprintf(stderr, "%s: Error during creating a new thread: %s\n", progname, strerror(errno));
                continue;
            }

            THREADS[slotIndex] = tid;
        }
    }

    if (closedir(dir) != 0)
    {
        fprintf(stderr, "%s: tid: %ld: Error during closing catalog. %s\n", progname, pthread_self(), strerror(errno));
    }
}

void removedir(const char *start)
{
    DIR *dir = opendir(start);
    if (dir == 0)
    {
        fprintf(stderr, "%s: tid: %ld: Can't open dir: [%s]: %s\n", progname, pthread_self(), start, strerror(errno));
        return;
    }

    struct dirent *current;

    while ((current = readdir(dir)) != NULL)
    {
        if (strcmp(current->d_name, ".") == 0 || strcmp(current->d_name, "..") == 0)
            continue;

        const char *currentPath = getFullPath(start, current->d_name);

        if (current->d_type == DT_DIR)
        {
            removedir(currentPath);
        }
        remove(currentPath);
    }
    remove(start);

    if (closedir(dir) != 0)
    {
        fprintf(stderr, "%s: tid: %ld: Error during closing catalog. %s\n", progname, pthread_self(), strerror(errno));
    }
}

int makeRoot(char *start)
{
    removedir(CYPHER_FOLDER);
    mkdir(CYPHER_FOLDER, 0777);
    int i = strlen(start) - 1;
    while (i > -1 && start[i] != '/')
        i--;

    if (i == -1)
        return 1;
    int lastSlashIndex = i;

    i = 1;
    while (i < strlen(start))
    {
        if (start[i] == '/')
        {
            start[i] = 0;
            char *buf = getFullPath(CYPHER_FOLDER, start + 1);
            mkdir(buf, 0777);
            start[i] = '/';
            free(buf);
        }
        i++;
    }
    char *buf = getFullPath(CYPHER_FOLDER, start + 1);
    mkdir(buf, 0777);
    free(buf);

    return 0;
}

void waitForAllFinished(int count)
{
    int hasInProgress;
    do
    {
        hasInProgress = 0;
        for (int i = 0; i < count; i++)
        {
            if (SLOT_STATUSES[i] == SS_IN_PROGRESS)
            {
                hasInProgress = 1;
                break;
            }
        }
    } while (hasInProgress);
}

void solveTask()
{
    fseek(keyFile, 0, SEEK_END);
    int size = ftell(keyFile);
    fseek(keyFile, 0, SEEK_SET);

    if (size < 0)
    {
        fprintf(stderr, "%s: tid: %ld: Key file is not a file.\n", progname, pthread_self());
        return;
    }
    if (size == 0)
    {
        fprintf(stderr, "%s: tid: %ld: Key file is empty.\n", progname, pthread_self());
        return;
    }

    key = calloc(size + 1, sizeof(char));
    for (int i = 0; i < size; i++)
    {
        key[i] = getc(keyFile);
    }
    wrappedCloseFile(keyFile);

    SLOT_STATUSES = calloc(maxThreadCount, sizeof(enum SLOT_STATUS));
    THREADS = calloc(maxThreadCount, sizeof(pthread_t));
    rec(startDir);

    waitForAllFinished(maxThreadCount);
    free(key);
}

int main(int argc, char *argv[])
{
    progname = basename(argv[0]);

    if (argc < REQ_ARG_COUNT)
    {
        fprintf(stderr, "%s: tid: %ld: Error. Count of missing argument: %d\n",
                progname, pthread_self(), REQ_ARG_COUNT - argc);
        fprintf(stderr, "Command should be: %s <start_catalog> <key_file> <max_count_of_processes>\n", progname);
        return -1;
    }

    startDir = argv[1];
    keyFile = fopen(argv[2], "r");
    char hasErrors = checkDir(startDir) || checkFile(keyFile) || checkNumber(argv[3]);

    if (hasErrors)
        return -1;

    maxThreadCount = atoi(argv[3]) - 1;

    tempOut = fopen(TEMP_RESULT_FILE, "w");

    if (makeRoot(startDir))
    {
        fprintf(stderr, "%s: tid %ld: Error while creating folder for cyphered files.\n", progname, pthread_self());
        return -1;
    }
    solveTask();

    wrappedCloseFile(tempOut);
#ifdef USE_TEMP_FILE
    int tempOutFD;
    char *buffer = calloc(BUFFER_SIZE, sizeof(char));
    if ((tempOutFD = open(TEMP_RESULT_FILE, O_RDONLY)))
    {
        int count;
        while ((count = read(tempOutFD, buffer, BUFFER_SIZE)) > 0)
        {
            printf("%s", buffer);
        }

        if (count == -1)
        {
            fprintf(stderr, "%s: tid: %ld: Error in file %s: %s\n", progname, pthread_self(), TEMP_RESULT_FILE, strerror(errno));
        }
    }
    free(buffer);
#endif

    return 0;
}
