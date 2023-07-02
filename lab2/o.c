/*2. Найти в заданном каталоге (аргумент 1 командной строки) и всех его
подкаталогах заданный файл (аргумент 2 командной строки). Вывести на консоль
полный путь к файлу, размер, дату создания, права доступа, номер индексного
дескриптора. Вывести также общее количество просмотренных каталогов и файлов.*/

#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#define STR_SIZE 512
#define RESULT_FILE "/tmp/result.txt"

char *exFile;
int countOfDir = 1;
int countOfFiles = 0;
FILE *file;

const char * monthStrings[] = {"Янв", "Фев", "Мар", "Апр", "Май", "Июнь",
			"Июль", "Авг", "Сент", "Окт", "Нояб", "Дек"};

char * getStringDate(struct tm * time) {
	char * date = calloc(16, sizeof(char));
	if(time->tm_mday < 10) {
		date[0] = '0';
		date[1] = '0' + time->tm_mday;
	} else {
		date[0] = '0' + ((int) (time->tm_mday / 10));
		date[1] = '0' + (time->tm_mday % 10); 
	}
	date[2] = ' ';
	strcat(date, monthStrings[time->tm_mon]);
	int index = strlen(date);
	int year = time->tm_year + 1900;
	date[index++] = ' ';
	date[index] = '0' + ((int) (year / 1000));
	date[index + 1] = '0' + ((int) (year / 100)) % 10;
	date[index + 2] = '0' + ((int) (year / 10)) % 10;
	date[index + 3] = '0' + (year % 10);
	return date;
}

void getSizeDateMode(struct stat *buffer, char *filepath)
{
    char *date;
    struct tm time;
    //date = (char *)malloc(sizeof(char)*STR_SIZE);
    localtime_r(&buffer->st_ctime, &time);
    //strftime(date, sizeof(char*)*STR_SIZE, "%d %b %Y", &time);
	date = getStringDate(&time);
    fprintf(file,"%s %ld %lo %ld %s\n",
            filepath, buffer->st_size, (unsigned long)buffer->st_mode, (long)buffer->st_ino, date);
    free(date);
}

int findDir(char *folder, char *fileName, int *fileEx){

    DIR *dirTemp;
    struct dirent *dir;
    struct stat *buffer = (struct stat*)calloc(1, sizeof(struct stat));
    char *fullPath, *filePath;
    int ret, temperrno;


    fullPath = (char *)malloc(sizeof(char)*STR_SIZE);
    filePath = (char *)malloc(sizeof(char)*STR_SIZE);

    if ((dirTemp = opendir(folder)) == NULL){

        fprintf(stderr, "%s : %s : %s\n", exFile, strerror(errno), realpath(folder, fullPath));
        return errno;
    }

    while((dir = readdir(dirTemp)) != NULL){

        if (dir->d_type != DT_DIR){

            if (dir->d_type == DT_REG) {
                countOfFiles++;

                if (strcmp(fileName, dir->d_name) == 0){

                    if (realpath(folder, filePath) != NULL){

                        strcat(filePath,"/");
                        strcat(filePath, fileName);

                    } else {
                        fprintf(stderr, "%s : %s : %s\n", exFile, strerror(errno), filePath);
                        continue;
                    }

                    if((ret = stat(filePath, buffer)) == 0) {
                        getSizeDateMode(buffer, filePath);
                    }
                }
            }
        }
        else{
            if (((dir->d_type == DT_DIR)) && ((strcmp(dir->d_name, ".")) != 0) && ((strcmp(dir->d_name, "..")) != 0)){
                countOfDir++;

                if (realpath(folder, filePath) != NULL){
                    strcat(filePath, "/");
                    strcat(filePath, dir->d_name);
                }

                else{
                    fprintf(stderr, "%s : %s : %s\n", exFile, strerror(errno), realpath(folder, fullPath));
                    return errno;
                }

                findDir(filePath, fileName, fileEx);
            }
        }
        temperrno = errno;
    }
    if (errno != temperrno){
        fprintf(stderr, "%s : %s : %s!\n", exFile, strerror(errno), realpath(folder, fullPath));
        return errno;
    }

    if (closedir(dirTemp) == -1)
    {
        fprintf(stderr, "%s : %s : %s!\n", exFile, strerror(errno), realpath(folder, fullPath));
        return errno;
    }

    free(buffer);
    free(fullPath);
    free(filePath);
}

char *getNameForErr(char *arg)
{
    int name = strlen(arg);
    int i = name - 1;

    while(arg[i] != '/')
        i--;

    int tempName = name - i;
    char *tempStr = malloc(sizeof(char)*(tempName));
    int j, k = 0;

    i++;

    for(j = i; j < name; j++)
        tempStr[k++] = arg[j];
    tempStr[k] = 0;

    return tempStr;
}

int main(int argc, char *argv[]){

    setlocale(LC_TIME, "ru_RU.UTF-8");
    exFile = getNameForErr(argv[0]);
    if(argc != 3){
        fprintf(stderr, "%s : count of arguments: must be 3 arguments!\n", exFile);
        return -1;
    }
    char *dirName, *fileName;
    int c, flag = 0;


    fileName = argv[2];
    dirName = argv[1];
    if ((file = fopen(RESULT_FILE, "w")) != NULL){
        findDir(dirName, fileName, &flag);
        fclose(file);
    }
    if ((file = fopen(RESULT_FILE, "r")) != NULL){
        while(1){
            c = fgetc(file);
            if ( feof(file) )
                break;
            printf("%c", c);
        }
        fclose(file);
    }
    else
        fprintf(stderr, "%s : %s : %s\n", exFile, strerror(errno), RESULT_FILE);

    printf("%d %d\n", countOfDir, countOfFiles);
    return 0;
}

