/*
Найти все дубликаты (с одинаковым содержимым) файлов в
заданном диапазоне размеров от N1 до N2 (N1, N2 задаются
в аргументах командной строки), начиная с исходного
каталога и ниже. Имя исходного каталога задаётся
пользователем в качестве первого аргумента командной строки.
*/

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>

#define REQ_ARG_COUNT 4


const char * progname;
const char * startDir;
int minSize, maxSize;


int isCorrectNumber(const char * num) {
	while(*num != 0) {
		if(*num < '0' || *num > '9') return 0;
		num++;
	}
	return 1;
}

int checkNumber(const char * num) {
	if(!isCorrectNumber(num)) {
		fprintf(stderr, "%s: Error. %s is invalid number.\n", progname, num);
		return 1;
	}
	return 0;
}

int checkDir(const char * path) {
	DIR * dir = opendir(path);
	if(dir == 0) {
		fprintf(stderr, "%s: Error. Invalid catalog. %s\n", progname, strerror(errno));
		return -1;
	}
	if(closedir(dir) != 0) {
		fprintf(stderr, "%s: Error during closing catalog. %s\n", progname, strerror(errno));
		return -1;
	}
	return 0;
}

int checkFile(FILE * file) {
	if(file == 0) {
		fprintf(stderr, "%s: Error. %s.\n", progname, strerror(errno));
		if(errno == 24) exit(1);
		return 1;
	}
	return 0;
}

void wrappedCloseFile(FILE * file) {
	if(fclose(file) != 0) {
		fprintf(stderr, "%s: Error during closing file. %s\n", progname, strerror(errno));
	}
}

int isFileEqual(const char * filename1, const char * filename2) {
	FILE * file1 = fopen(filename1, "r");
	FILE * file2 = fopen(filename2, "r");

	if(checkFile(file1) || checkFile(file2)) return 0;

	char c;
	while((c = getc(file1)) != EOF) {
		if(c != getc(file2)) {
			wrappedCloseFile(file1);
			wrappedCloseFile(file2);
			return 0;
		}
	}

	wrappedCloseFile(file1);
	wrappedCloseFile(file2);
	return 1;
}

char * getFullPath(const char * path, const char * name) {
	char * answer = calloc(strlen(path) + strlen(name) + 2, sizeof(char));
	strcpy(answer, path);
	if(answer[strlen(answer) - 1] != '/')
		strcat(answer, "/");
	strcat(answer, name);
	return answer;
}

typedef struct SFile {
	long size;
	char * path;
	struct SFile * next;
	struct SFile * prev;
} * File;

void addToList(File begin, const char * path, long size) {
	if(begin == NULL) return;

	if(begin->path == NULL) {
		begin->path = (char *) calloc(strlen(path) + 1, sizeof(char));
		strcpy(begin->path, path);
		begin->size = size;
	} else {
		File current = begin;
		while(current->next != NULL) current = current->next;
		current->next = calloc(1, sizeof(struct SFile));

		current->next->path = (char *) calloc(strlen(path) + 1, sizeof(char));
		strcpy(current->next->path, path);
		current->next->size = size;
		current->next->prev = current;
	}
}

void freeList(File begin) {
	if(begin == NULL) return;
	File next = begin->next;

	while(next != NULL) {
		free(begin->path);
		free(begin);

		begin = next;
		next = begin->next;
	}

	free(begin->path);
	free(begin);
}

void rec(const char * start, File lastFile) {
	DIR *dir = opendir(start);
	if(dir == 0) {
		printf("%s: Can't open dir: [%s]: %s\n", progname, start, strerror(errno));
		return;
	}

	struct dirent * current;
	struct stat * info = (struct stat *) calloc(1, sizeof(struct stat));

	while((current = readdir(dir)) != NULL) {
		if(strcmp(current->d_name, ".") == 0
		|| strcmp(current->d_name, "..") == 0) continue;

		const char * currentPath = getFullPath(start, current->d_name);

		if(stat(currentPath, info)) {
			fprintf(stderr, "%s: Error while getting stat of [%s]: %s\n", progname, currentPath, strerror(errno));
			continue;
		}

		if(S_ISDIR(info->st_mode)) {
			rec(currentPath, lastFile);
			continue;
		}

		if(S_ISREG(info->st_mode)
		&& info->st_size >= minSize
		&& info->st_size <= maxSize) {
			addToList(lastFile, currentPath, info->st_size);
		}
	}

	free(info);
	if(closedir(dir) != 0) {
		fprintf(stderr, "%s: Error during closing catalog. %s\n", progname, strerror(errno));
	}
}

File removeFile(File file) {
	if(file->prev != NULL) {
		file->prev->next = file->next;
	}
	if(file->next != NULL) {
		file->next->prev = file->prev;
	}
	File next = file->next;
	free(file);
	return next;
}

void solveTask() {
	File firstFile = (File) calloc(1, sizeof(struct SFile));
	rec(startDir, firstFile);

	File firstIterator = firstFile;
	while(firstIterator != NULL) {
		File secondIterator = firstIterator;
		while(secondIterator != NULL) {
			if(firstIterator->size == secondIterator->size
			&& strcmp(firstIterator->path, secondIterator->path) != 0
			&& (firstIterator->size == 0 || isFileEqual(firstIterator->path, secondIterator->path))) {
				printf("[%s] == [%s]\n", firstIterator->path, secondIterator->path);
				secondIterator = removeFile(secondIterator);
			}
			if(secondIterator != NULL)
				secondIterator = secondIterator->next;
		}
		firstIterator = firstIterator->next;
	}

	freeList(firstFile);
}

int main(int argc, char * argv[]) {
	progname = basename(argv[0]);

	if(argc < REQ_ARG_COUNT) {
		fprintf(stderr, "%s: Error. Count of missing argument: %d\nCommand should be: %s <start_catalog> <min_size> <max_size>\n", 
			progname, REQ_ARG_COUNT - argc, progname);
		return -1;
	}

	startDir = argv[1];
	char hasErrors = checkDir(startDir);

	minSize = atol(argv[2]);
	hasErrors = hasErrors || checkNumber(argv[2]);
	maxSize = atol(argv[3]);
	hasErrors = hasErrors || checkNumber(argv[3]);

	if(hasErrors) return -1;
	solveTask();

	return 0;
}
