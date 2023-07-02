/*

Написать программу копирования одного файла в другой.
В качестве параметров при вызове программы передаются
имена первого и второго файлов. Для чтения или записи
файла использовать только функции посимвольного ввода-
вывода getc(),putc(), fgetc(),fputc(). Предусмотреть
копирование прав доступа к файлу и контроль ошибок
открытия/закрытия/чтения/записи файл

*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

struct MyFile {
	FILE * file;
	const char * filename;
};

void copyFile(struct MyFile src, struct MyFile dest) {
	struct stat srcStat;

	stat(src.filename, &srcStat);
	chmod(dest.filename, srcStat.st_mode);

	int c;
	while((c = getc(src.file)) != EOF) {
		putc(c, dest.file);
	}
}

int main(int argc, char * argv[]) {
	if(argc < 3) {
		fprintf(stderr, "%s: Error. Count of missing argument: %d\n", argv[0], 3 - argc);
		return -1;
	}

	char hasErrors = 0;
	FILE * srcFile = fopen(argv[1], "r");
	if(srcFile == 0) {
		fprintf(stderr, "%s: Error. Invalid source file.\n", argv[0]);
		hasErrors = 1;
	}

	FILE * destFile = fopen(argv[2], "w");
	if(destFile == 0) {
		fprintf(stderr, "%s: Error. Invalid destination file.\n", argv[0]);
		hasErrors = 1;
	}
	if(hasErrors) return -1;

	struct MyFile src = {srcFile, argv[1]};
	struct MyFile dest = {destFile, argv[2]};

	copyFile(src, dest);

	if(fclose(src.file) != 0 || fclose(dest.file) != 0) {
		fprintf(stderr, "%s: Error during closing file.\n", argv[0]);
		return -1;
	}

	return 0;
}
