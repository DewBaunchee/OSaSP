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
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <time.h>
#include <sys/shm.h>

#define REQ_ARG_COUNT 4
#define TEMP_RESULT_FILE "/tmp/tempResult.txt"
#define CYPHER_FOLDER "/tmp/cyphered"
#define BUFFER_SIZE 16 * 1024 * 1024
#define NSOPS 1

//#define USE_TEMP_FILE
//#define DEBUGING
//#define SEMAPHORE_OPS
//#define PRINT_DURATION

// LIST OF FILE
typedef struct SFile
{
    char *pfile;
    char *cfile;
    struct SFile *next;
} * File;

typedef struct SFilePaths
{
    char ppath[PATH_MAX];
    char cpath[PATH_MAX];
} * FilePaths;

typedef struct SList
{
    File begin;
    File end;
    int count;
} * List;

void addToList(List list, const char *pfile, const char *cfile)
{
    if (list == NULL)
        return;

    if (list->begin == NULL)
    {
        list->begin = list->end = calloc(1, sizeof(struct SFile));

        list->begin->pfile = (char *)calloc(strlen(pfile) + 1, sizeof(char));
        strcpy(list->begin->pfile, pfile);

        list->begin->cfile = (char *)calloc(strlen(cfile) + 1, sizeof(char));
        strcpy(list->begin->cfile, cfile);

        list->count = 1;
    }
    else
    {
        list->end->next = calloc(1, sizeof(struct SFile));

        list->end->next->pfile = (char *)calloc(strlen(pfile) + 1, sizeof(char));
        strcpy(list->end->next->pfile, pfile);

        list->end->next->cfile = (char *)calloc(strlen(cfile) + 1, sizeof(char));
        strcpy(list->end->next->cfile, cfile);

        list->end = list->end->next;

        list->count++;
    }
}

void freeList(List list)
{
    if (list->begin == NULL)
        return;
    File next = list->begin->next;

    while (next != NULL)
    {
        free(list->begin->pfile);
        free(list->begin->cfile);
        free(list->begin);

        list->begin = next;
        next = list->begin->next;
    }

    free(list->begin->pfile);
    free(list->begin->cfile);
    free(list->begin);
    free(list);
}
// END LIST OF FILE

int isCorrectNumber(const char *);
int checkNumber(const char *);
int isDigit(char);
int checkDir(const char *);
int checkFile(FILE *);
int wrappedCloseFile(FILE *);
int wrappedCloseFileDescriptor(int);
char *getFullPath(const char *, const char *);
int fileEncrypt(const char *, const char *, const char *);
void rec(const char *, List);
void encrypt(List, const char *);
void removedir(const char *);
void solveTask();
const char *getPathByFD(int);
int makeRoot(char *);

int getSemaphore();
void removeSemaphore(int);
int getSemValue(int);
void addSemaphoreValue(int, int);

void waitForWriting();
void waitForReading();
void writeBegin();
void writeEnd();
void readEnd();

const char *progname;
char *startDir;
int maxProcessCount;
int readerSemaphore;
int writerSemaphore;
FilePaths fpaths;

FILE *keyFile;
FILE *tempOut;

int getSemaphore()
{
    int sem = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (sem == -1)
    {
        fprintf(stderr, "%s: pid %d: Error while creating semaphore: %s\n", progname, getpid(), strerror(errno));
        exit(EXIT_FAILURE);
    }
    return sem;
}

void removeSemaphore(int sem)
{
    semctl(sem, 0, IPC_RMID);
}

int getSemValue(int semaphore)
{
    int count = semctl(semaphore, 0, GETVAL);
#ifdef DEBUGING
    printf("Process count: %d, max = %d\n", count, maxProcessCount);
#endif
    return count;
}

void addSemaphoreValue(int semaphore, int value)
{
    struct sembuf op = {0, value, 0};
    int notComplete = 1;

    while (notComplete)
    {
        if (semop(semaphore, &op, NSOPS))
        {
            fprintf(stderr, "%s: pid %d: Error while adding %d to semaphore: %s\n", progname, getpid(), value, strerror(errno));
        }
        else
        {
            notComplete = 0;
        }
    }
}

int fileEncrypt(const char *plainFile, const char *cypheredFile, const char *key)
{
    int keySize = strlen(key);
    int keyIndex = 0;
    int countOfBytes = 0;
    int hasErrors = 0;

    int pfile = open(plainFile, O_RDONLY);
    int cfile = open(cypheredFile, O_CREAT | O_WRONLY);

    if (pfile && cfile)
    {
        char *buffer = calloc(BUFFER_SIZE, sizeof(char));
        char *cyphered = calloc(BUFFER_SIZE, sizeof(char));
        int count;

        while ((count = read(pfile, buffer, BUFFER_SIZE)) > 0)
        {
            for (int i = 0; i < count; i++)
            {
                cyphered[i] = buffer[i] ^ key[keyIndex];
                keyIndex = (keyIndex + 1) % keySize;
            }
            countOfBytes += count;

            if (write(cfile, cyphered, count) != count)
            {
                fprintf(stderr, "%s: pid: %d: Error while writing in file %s: %s\n", progname, getpid(), cypheredFile, strerror(errno));
                hasErrors = 1;
                break;
            }
        }

        if (count == -1)
        {
            fprintf(stdout, "%s: pid: %d: Error while reading file %s: %s\n", progname, getpid(), plainFile, strerror(errno));
            hasErrors = 1;
        }

        hasErrors = hasErrors || wrappedCloseFileDescriptor(pfile) || wrappedCloseFileDescriptor(cfile);

        close(pfile);
        close(cfile);
        free(cyphered);
        free(buffer);

        if (!hasErrors)
        {
#ifdef USE_TEMP_FILE
            fprintf(tempOut, "%d %s %d\n", getpid(), plainFile, countOfBytes);
#else
            printf("%d %s %d\n", getpid(), plainFile, countOfBytes);
#endif
        }
    }
    else
    {
        fprintf(stderr, "%s: pid: %d: Can't open file: %s\n", progname, getpid(), strerror(errno));
        hasErrors = 1;
    }

    return hasErrors;
}

void sigintHandler(int signum)
{
    while (wait(NULL) != -1)
        ;
    removeSemaphore(readerSemaphore);
    removeSemaphore(writerSemaphore);
    shmdt(fpaths);
    exit(-1);
}

void sigtstpHandler(int signum)
{
    char call[256];
    sprintf(call, "ps f -C %s", progname);
    system(call);
}

void waitForWriting()
{
#ifdef SEMAPHORE_OPS
    printf("Wait for writing\n");
#endif
    addSemaphoreValue(writerSemaphore, -1);
}

void waitForReading()
{
#ifdef SEMAPHORE_OPS
    printf("Wait for reading\n");
#endif
    addSemaphoreValue(readerSemaphore, 0);
}

void writeBegin()
{
#ifdef SEMAPHORE_OPS
    printf("Write begin\n");
#endif
    addSemaphoreValue(readerSemaphore, 1);
}

void writeEnd()
{
#ifdef SEMAPHORE_OPS
    printf("Write end\n");
#endif
    addSemaphoreValue(writerSemaphore, 1);
}

void readEnd()
{
#ifdef SEMAPHORE_OPS
    printf("Read end\n");
#endif
    addSemaphoreValue(readerSemaphore, -1);
}

void encrypt(List list, const char *key)
{
    signal(SIGCHLD, SIG_IGN);

    int id = shmget(IPC_PRIVATE, sizeof(struct SFilePaths), IPC_CREAT | 0644);

    for (int i = 0; i < maxProcessCount; i++)
    {
        switch (fork())
        {
        case -1:
            fprintf(stderr, "%s: pid: %d: Error during fork: %s\n", progname, getpid(), strerror(errno));
            break;
        case 0:
        {
            fpaths = (FilePaths)shmat(id, NULL, 0);

            while (1)
            {
                waitForWriting();
                if (fpaths->ppath[0] == 0)
                    break;

                char *plainFile = calloc(strlen(fpaths->ppath) + 1, sizeof(char));
                char *cypheredFile = calloc(strlen(fpaths->cpath) + 1, sizeof(char));
                strcpy(plainFile, fpaths->ppath);
                strcpy(cypheredFile, fpaths->cpath);
                readEnd();

#ifdef DEBUGING
                printf("Encrypting\n");
#endif

                fileEncrypt(plainFile, cypheredFile, key);
            }

            shmdt(fpaths);

#ifdef DEBUGING
            printf("Exiting\n");
#endif
            exit(0);
        }
        }
    }

    fpaths = (FilePaths)shmat(id, NULL, 0);
    File current = list->begin;
    while (current != NULL)
    {
        waitForReading();
        writeBegin();
        strcpy(fpaths->ppath, current->pfile);
        strcpy(fpaths->cpath, current->cfile);
        writeEnd();
        current = current->next;
    }
    addSemaphoreValue(writerSemaphore, maxProcessCount);
    fpaths->ppath[0] = 0;

    while (wait(NULL) != -1)
        ;
    shmdt(fpaths);
}

void rec(const char *start, List list)
{
    DIR *dir = opendir(start);
    if (dir == 0)
    {
        fprintf(stderr, "%s: pid: %d: Can't open dir: [%s]: %s\n", progname, getpid(), start, strerror(errno));
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
            rec(currentPath, list);
            continue;
        }

        if (current->d_type == DT_REG)
        {
#ifdef DEBUGING
            printf("Adding to list: %s\n", currentPath);
#endif
            addToList(list, currentPath, getFullPath(CYPHER_FOLDER, currentPath + 1));
        }
    }

    if (closedir(dir) != 0)
    {
        fprintf(stderr, "%s: pid: %d: Error during closing catalog. %s\n", progname, getpid(), strerror(errno));
    }
}

#ifdef PRINT_DURATION
time_t duration;
#endif

void solveTask()
{
    fseek(keyFile, 0, SEEK_END);
    int size = ftell(keyFile);
    fseek(keyFile, 0, SEEK_SET);

    if (size < 0)
    {
        fprintf(stderr, "%s: pid: %d: Key file is not a file.\n", progname, getpid());
        return;
    }
    if (size == 0)
    {
        fprintf(stderr, "%s: pid: %d: Key file is empty.\n", progname, getpid());
        return;
    }

    char *key = calloc(size + 1, sizeof(char));
    for (int i = 0; i < size; i++)
    {
        key[i] = getc(keyFile);
    }
    wrappedCloseFile(keyFile);

#ifdef PRINT_DURATION
    time_t start = time(NULL);
#endif

    List list = calloc(1, sizeof(struct SList));
    rec(startDir, list);

 //   signal(SIGTSTP, &sigtstpHandler);
    signal(SIGINT, &sigintHandler);
    readerSemaphore = getSemaphore();
    writerSemaphore = getSemaphore();

    encrypt(list, key);

    removeSemaphore(readerSemaphore);
    removeSemaphore(writerSemaphore);

#ifdef PRINT_DURATION
    duration = time(NULL) - start;
#endif

    free(key);
    freeList(list);
}

int main(int argc, char *argv[])
{
    progname = basename(argv[0]);

    if (argc < REQ_ARG_COUNT)
    {
        fprintf(stderr, "%s: pid: %d: Error. Count of missing argument: %d\n",
                progname, getpid(), REQ_ARG_COUNT - argc);
        fprintf(stderr, "Command should be: %s <start_catalog> <key_file> <max_count_of_processes>\n", progname);
        return -1;
    }

    startDir = argv[1];
    keyFile = fopen(argv[2], "r");
    char hasErrors = checkDir(startDir) || checkFile(keyFile) || checkNumber(argv[3]);

    maxProcessCount = atoi(argv[3]);
    if (maxProcessCount < 1)
    {
        fprintf(stderr, "%s: Error: max count of processes must be 1 or bigger\n", progname);
        hasErrors = 1;
    }

    if (hasErrors)
        return -1;

    maxProcessCount--;

#ifdef USE_TEMP_FILE
    tempOut = fopen(TEMP_RESULT_FILE, "w");
#endif

    if (makeRoot(startDir))
    {
        fprintf(stderr, "%s: pid %d: Error while creating folder for cyphered files.\n", progname, getpid());
        return -1;
    }

    solveTask();

#ifdef USE_TEMP_FILE
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
#endif
#ifdef PRINT_DURATION
    printf("Duration: %lu\n", duration);
#endif

    return 0;
}

// OTHERS

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
        fprintf(stderr, "%s: pid: %d: Error. %s is invalid number of process.\n", progname, getpid(), num);
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
        fprintf(stderr, "%s: pid: %d: Error. Invalid catalog. %s\n", progname, getpid(), strerror(errno));
        return -1;
    }
    if (closedir(dir) != 0)
    {
        fprintf(stderr, "%s: pid: %d: Error during closing catalog. %s\n", progname, getpid(), strerror(errno));
        return -1;
    }
    return 0;
}

int checkFile(FILE *file)
{
    if (file == 0)
    {
        fprintf(stderr, "%s: pid: %d: Error. %s.\n", progname, getpid(), strerror(errno));
        if (errno == 24)
            exit(1);
        return 1;
    }
    return 0;
}

int wrappedCloseFile(FILE *file)
{
    if (fclose(file) != 0)
    {
        fprintf(stderr, "%s: pid: %d: Error during closing file. %s\n", progname, getpid(), strerror(errno));
        return 1;
    }
    return 0;
}

int wrappedCloseFileDescriptor(int fd)
{
    if (close(fd) != 0)
    {
        fprintf(stderr, "%s: pid: %d: Error during closing file descriptor. %s\n", progname, getpid(), strerror(errno));
        return 1;
    }
    return 0;
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

void removedir(const char *start)
{
    DIR *dir = opendir(start);
    if (dir == 0)
    {
        fprintf(stderr, "%s: pid: %d: Can't open dir: [%s]: %s\n", progname, getpid(), start, strerror(errno));
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
        fprintf(stderr, "%s: pid: %d: Error during closing catalog. %s\n", progname, getpid(), strerror(errno));
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