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

#define REQ_ARG_COUNT 4
#define TEMP_RESULT_FILE "/tmp/tempResult.txt"
#define CYPHER_FOLDER "/tmp/cyphered"
#define BUFFER_SIZE 1024 * 1024

const char *progname;
char *startDir;
FILE *keyFile;
int maxProcessCount;

FILE *tempOut;

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

void wrappedCloseFile(FILE *file)
{
    if (fclose(file) != 0)
    {
        fprintf(stderr, "%s: pid: %d: Error during closing file. %s\n", progname, getpid(), strerror(errno));
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

int processCount;

void forkEncrypt(const char *plainFile, const char *cypheredFile, const char *key)
{
    switch (fork())
    {
    case -1:
        fprintf(stderr, "%s: pid: %d: Error during fork: %s\n", progname, getpid(), strerror(errno));
        break;
    case 0:
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
                    fprintf(stderr, "%s: pid: %d: Error while writing in file %s: %s\n", progname, getpid(), cypheredFile, strerror(errno));
                    exit(-1);
                }
            }

            if (count == -1)
            {
                fprintf(stderr, "%s: pid: %d: Error in file: %s\n", progname, getpid(), plainFile);
            }

            int hasErrors = 0;
            int status = close(pfile);
            if (status == -1)
            {
                hasErrors = 1;
                fprintf(stderr, "%s: pid: %d: Error while closing file %s: %s\n", progname, getpid(), plainFile, strerror(errno));
            }

            status = close(cfile);
            if (status == -1)
            {
                hasErrors = 1;
                fprintf(stderr, "%s: pid: %d: Error while closing file %s: %s\n", progname, getpid(), cypheredFile, strerror(errno));
            }

            if (hasErrors)
                exit(-1);

            fprintf(tempOut, "%d %s %d\n", getpid(), plainFile, countOfBytes);
            free(buffer);
        }
        else
        {
            fprintf(stderr, "%s: pid: %d: Can't open file: %s\n", progname, getpid(), strerror(errno));
        }
        exit(EXIT_SUCCESS);
    }
    default:
        processCount++;
    }
}

void rec(const char *start, const char *key)
{
    processCount = 0;
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
            rec(currentPath, key);
            continue;
        }

        if (current->d_type == DT_REG)
        {
            if (processCount >= maxProcessCount)
            {
                wait(NULL);
                processCount--;
            }

            forkEncrypt(currentPath, getFullPath(CYPHER_FOLDER, currentPath + 1), key);
        }
    }

    if (closedir(dir) != 0)
    {
        fprintf(stderr, "%s: pid: %d: Error during closing catalog. %s\n", progname, getpid(), strerror(errno));
    }

    while (wait(NULL) != -1)
        ;
}

void removedir(const char *start)
{
    processCount = 0;
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

    while (wait(NULL) != -1)
        ;
}

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

    rec(startDir, key);

    free(key);
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

    if (hasErrors)
        return -1;

    maxProcessCount = atoi(argv[3]) - 1;

    tempOut = fopen(TEMP_RESULT_FILE, "w");

    if (makeRoot(startDir))
    {
        fprintf(stderr, "%s: pid %d: Error while creating folder for cyphered files.\n", progname, getpid());
        return -1;
    }
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

    return 0;
}
