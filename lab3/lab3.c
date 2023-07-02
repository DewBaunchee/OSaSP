/*
Написать программу поиска одинаковых по содержимому файлов в двух каталогов, 
например, Dir1 и Dir2. Пользователь задаёт имена Dir1 и Dir2. В результате 
работы программы файлы, имеющиеся в Dir1, сравниваются с файлами в Dir2 по их 
содержимому. Процедуры сравнения должны запускаться в отдельном процессе для 
каждой пары сравниваемых файлов. Каждый процесс выводит на экран свой pid, 
имя файла, общее число просмотренных байт и результаты сравнения. 
Число одновременно работающих процессов не должно превышать N 
(вводится пользователем). Скопировать несколько файлов из каталога 
/etc в свой домашний каталог. Проверить работу программы для каталога 
/etc и домашнего каталога.
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

#define REQ_ARG_COUNT 4
#define TEMP_RESULT_FILE "/home/dewbaunchee/951007/VorivodaMatvey/lab3/tempResult.txt"

const char *progname;
const char *startDir1;
const char *startDir2;
int maxProcessCount;

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

// LIST OF FILE
typedef struct SFile
{
	char *path;
	struct SFile *next;
} * File;

typedef struct SList
{
	File begin;
	File end;
} * List;

void addToList(List list, const char *path)
{
	if (list == NULL)
		return;

	if (list->begin == NULL)
	{
		list->begin = list->end = calloc(1, sizeof(struct SFile));
		list->begin->path = (char *)calloc(strlen(path) + 1, sizeof(char));
		strcpy(list->begin->path, path);
	}
	else
	{
		list->end->next = calloc(1, sizeof(struct SFile));

		list->end->next->path = (char *)calloc(strlen(path) + 1, sizeof(char));
		strcpy(list->end->next->path, path);
		list->end = list->end->next;
	}
}

void freeList(List list)
{
	if (list->begin == NULL)
		return;
	File next = list->begin->next;

	while (next != NULL)
	{
		free(list->begin->path);
		free(list->begin);

		list->begin = next;
		next = list->begin->next;
	}

	free(list->begin->path);
	free(list->begin);
	free(list);
}
// END LIST OF FILE

char *getFullPath(const char *path, const char *name)
{
	char *answer = calloc(strlen(path) + strlen(name) + 2, sizeof(char));
	strcpy(answer, path);
	if (answer[strlen(answer) - 1] != '/')
		strcat(answer, "/");
	strcat(answer, name);
	return answer;
}

void rec(const char *start, List list)
{
	DIR *dir = opendir(start);
	if (dir == 0)
	{
		printf("%s: Can't open dir: [%s]: %s\n", progname, start, strerror(errno));
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
			rec(currentPath, list);
			continue;
		}

		if (current->d_type == DT_REG)
		{
			addToList(list, currentPath);
		}
	}

	if (closedir(dir) != 0)
	{
		fprintf(stderr, "%s: Error during closing catalog. %s\n", progname, strerror(errno));
	}
}

FILE *tempOut;

void cmpFiles(File file1, File file2)
{
	FILE *f1 = fopen(file1->path, "r");
	FILE *f2 = fopen(file2->path, "r");
	if (f1 == 0 || f2 == 0)
	{
		fprintf(stderr, "%s: Error while open file: %s\n", progname, strerror(errno));
	}

	char isEqual = 1;
	long countOfBytes = 0;

	while (1)
	{
		char c1 = getc(f1);
		char c2 = getc(f2);

		if (feof(f1) || feof(f2))
		{
			if (!(feof(f1) && feof(f2)))
				isEqual = 0;
			break;
		}

		countOfBytes++;
		if (c1 != c2)
		{
			isEqual = 0;
			break;
		}
	}

	fprintf(tempOut, "%d %s %s %ld %s\n", getpid(), file1->path, file2->path, countOfBytes, isEqual ? "Equal" : "Not equal");

	fclose(f1);
	fclose(f2);
}

void compareLists(List list1, List list2)
{
	File file1 = list1->begin;
	int pidCount = 0;

	while (file1 != NULL)
	{
		File file2 = list2->begin;
		while (file2 != NULL)
		{

			if (pidCount++ >= maxProcessCount)
			{
				wait(NULL);
			}

			switch (fork())
			{
			case -1:
				fprintf(stderr, "%s: Error during fork: %s", progname, strerror(errno));
				break;
			case 0:
				cmpFiles(file1, file2);
				freeList(list1);
				freeList(list2);
				exit(0);
			}

			file2 = file2->next;
		}
		file1 = file1->next;
	}
}

time_t duration;

void solveTask()
{
	List list1 = calloc(1, sizeof(struct SList));
	List list2 = calloc(1, sizeof(struct SList));
	rec(startDir1, list1);
	rec(startDir2, list2);
	
	time_t start = time(NULL);
	compareLists(list1, list2);
	duration = time(NULL) - start;

	freeList(list1);
	freeList(list2);
}

int main(int argc, char *argv[])
{
	progname = basename(argv[0]);

	if (argc < REQ_ARG_COUNT)
	{
		fprintf(stderr, "%s: Error. Count of missing argument: %d\n",
				progname, REQ_ARG_COUNT - argc);
		fprintf(stderr, "Command should be: %s <first_directory> <second_directory> <max_count_of_processes>\n", progname);
		return -1;
	}

	startDir1 = argv[1];
	startDir2 = argv[2];
	char hasErrors = checkDir(startDir1) || checkDir(startDir2) || checkNumber(argv[3]);

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
	printf("Duration: %lds\n", duration);

	wrappedCloseFile(tempOut);

	return 0;
}
