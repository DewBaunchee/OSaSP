#include <sys/types.h>

#include <sys/stat.h>

#include <unistd.h>

#include <stdio.h>

#include <string.h>

#include <stdlib.h>

#include <dirent.h>

#include <errno.h>

#include <memory.h>

/*----------------------------------------------

Функция GETKOL рекурсивная. Возвращает количичестов

файлов по заданной директории и поддерикториях.

----------------------------------------------*/

int getkol(char *dirn) // Функция возращает количество рег. файлов в заданном каталоге включая подкаталоге

{

    int dsize = 0; //колич. файлов

    DIR *dirs; //создаем указатель на поток директории

    //if(*(dirn+(int)strlen(dirn)-1)==92) *(dirn+(int)strlen(dirn)-1)=0;

    dirs = opendir(dirn); //открваем директорию с именем dirn

    if (dirs) // если удается создать поток (открыть директорию)

    {

        struct dirent *dirstr; //создаем указатель на структуру для хранения

        // элемента из потока директории

        while (dirstr = readdir(dirs)) //пока удается получаем файл из потока

        {

            char *fnm = dirstr->d_name; //получаем из структуры имя файла

            char *fullnm = (char *)alloca(strlen(dirn) + strlen(fnm) + 1); //выделяем память по полное имя

            //файла (путь + имя)

            strcpy(fullnm, dirn); //операции для получения полного имени

            strcat(fullnm, "/");

            strcat(fullnm, fnm);

            struct stat fst; //структура для хранения информации о файле

            stat(fullnm, &fst); //получаем информацию о файле в структуру

            if (S_ISREG(fst.st_mode) && !S_ISLNK(fst.st_mode)) //Увеличиваем счетчик если файл

            // регулярный и не является ссылкой (!второе условие не срабатывает (Ubuntu 10.04))

            {

                dsize++;
            }
            else

                if (S_ISDIR(fst.st_mode) && strcmp(fnm, ".") && strcmp(fnm, "..") && !S_ISLNK(fst.st_mode)) //Если

            //файл является каталогом и не является ссылкой(не сработало!).

            // strcmp возвращает 0 если строки равны.

            {

                dsize = dsize + getkol(fullnm); //вызваем функцию рекурсивно
            }
        }

        closedir(dirs); //закрываем каталог
    }

    else
        printf("Couldn't open derictory - %s\n", dirn);

    return dsize;
}

/*----------------------------------------------

Функция GETLIST рекурсивна, на переданный указатель

записывает список имен файлов по заданому имени

каталога, а также заполняет соответственно массив

размеров каждого файла.

Входной параметр функциц "_i" служит для получения

числа фактически считанных файлов.

----------------------------------------------*/

void getlist(int *_i, char **_str, char *dirn, int *_fsize)

{

    DIR *dirs;

    dirs = opendir(dirn);

    if (dirs)

    {

        struct dirent *dirstr;

        while (dirstr = readdir(dirs))

        {

            char *fnm = dirstr->d_name; //получаем из структуры имя файла

            char *fullnm = (char *)alloca(strlen(dirn) + strlen(fnm) + 1);

            strcpy(fullnm, dirn);

            strcat(fullnm, "/");

            strcat(fullnm, fnm);

            struct stat fst;

            stat(fullnm, &fst);

            if (S_ISREG(fst.st_mode) && !S_ISLNK(fst.st_mode))

            {

                strcpy(_str[*_i], fullnm); //копирование полного имени файла в массив

                *(_fsize + *_i) = fst.st_size; //заполняем массив размера файла

                (*_i)++;
            }

            if (S_ISDIR(fst.st_mode) && strcmp(fnm, ".") && strcmp(fnm, "..") && !S_ISLNK(fst.st_mode))

            {

                getlist(_i, _str, fullnm, _fsize); //если файл каталог функция вызывает саму себя
            }
        }

        closedir(dirs);
    }

    else
        printf("Couldn't open derictory - %s\n", dirn);
}

/*----------------------------------------------

Функция создает новый процесс в котором сравниваются

два фала. Входные параметры: имя первого, имя второго и размер

файла. В функцию буду передаватся только файлы одинакового размера.

----------------------------------------------*/

int xfork(char *fnm1, char *fnm2, int *_fs)

{

    int ppid = fork(); //создается новый дочерний процесс

    if (ppid == 0) //если процесс ребенок

    {

        int t = 0;

        FILE *fd1 = fopen(fnm1, "r");

        FILE *fd2 = fopen(fnm2, "r");

        while (abs(fgetc(fd1)) == fgetc(fd2))
            t++; //сравниваем посимвольно

        //выводим форматированную строку с результатом

        printf("\n_____ PID: %d ____\n1> %s\n2> %s\n === Result: %d of %d bytes match ===\n", getpid(), fnm1, fnm2, t, *_fs);

        fclose(fd1);

        fclose(fd2);
    }

    return ppid;
}

/*----------------------------------------------

Главная функция. Входные параметры получаем из командной строки.

----------------------------------------------*/

main(int argc, char *argv[])

{

    if (argc > 4) //Если задано параметров больше чем требуетсяэ

    {

        puts("Put that info in argv: DIR1 DIR2 N");

        exit(1);
    }

    char *fnm1 = argv[1]; //Получаем имена файлов.

    char *fnm2 = argv[2];

    int N;

    if (!sscanf(argv[3], "%d", &N)) //Если вместо числа одновременно

    { //запущеных процессоввведена строка.

        puts("Not whole number");

        exit(1);
    }

    errno = 0; //Обнуляем переменную ошибок описанную в заголовочом файле.

    char **str1; // Список файлов в директориях, поддерикториях.

    char **str2;

    int *fsize1; // Массив размеров.

    int *fsize2; // Массив размеров.

    int ksize1 = getkol(argv[1]); //Подсчитываем файлы в 2ух каталогах

    int ksize2 = getkol(argv[2]);

    printf("Total files found: %d ", ksize1 + ksize2);

    /*----------------------------------------------

Далее идет процесс выделения памяти под необходимые данные

----------------------------------------------*/

    str1 = malloc(ksize1 * (sizeof(char *)));

    str2 = malloc(ksize2 * (sizeof(char *)));

    fsize1 = malloc(ksize1 * (sizeof(int)));

    fsize2 = malloc(ksize2 * (sizeof(int)));

    int i = 0; //переменная счетчик

    while (i < ksize1)

    {

        *(str1 + i) = malloc(512 * sizeof(char));

        i++;
    }

    i = 0;

    while (i < ksize2)

    {

        *(str2 + i) = malloc(512 * sizeof(char));

        i++;
    }

    ksize1 = 0;

    ksize2 = 0; //обнуляем размер

    getlist(&ksize1, str1, argv[1], fsize1); //получаем список имен и размеров файлов

    getlist(&ksize2, str2, argv[2], fsize2);

    printf("Total files read: %d\n", ksize1 + ksize2);

    /*i=0;

while(i<ksize)//Выводим список файлов и размер каждого

{

printf("%s -- %d bytes\n",*(str+i),*(size+i));

i++;

}*/

    i = 0; //обнуляем счетчик

    int fr = 1; //результат работы фокр

    int k, n = 0; // переменные счетчики

    while (i < ksize1) //в этом цикле берется итый размер файла из первого каталога

    {

        k = 0; //катый размер во 2ом каталоге

        while (k < ksize2) //катый размер 2ого каталога

        {

            if (fr > 0) //если процесс родетельский

            {

                if (*(fsize1 + i) == *(fsize2 + k)) //если совпали размеры

                {

                    if (n++ > N)
                        wait(); //Если запущено больше N процессов

                    //ждем первого завершивсегося.

                    fr = xfork(*(str1 + i), *(str2 + k), fsize1 + i);

                    //Вызвали функцию сравнения в новом процессе.
                }

                k++;
            }
            else
                break; //если процесс не родительский прерываем цикл
        }

        i++;
    }

    if (fr > 0) //если процесс родетельский

    {

        while (wait() > 0)
            ; //ожидание завершения всех доч. процессов.

        char *err = strerror(errno); //проверяме переменную ошибок

        printf("\nErrno result: %s \n", err); //Выводим отчет об ошибках.
    }

    //далее освобождаем динамическую память

    i = 0;

    while (i < ksize1)

    {

        free(*(str1 + i));

        i++;
    }

    while (i < ksize2)

    {

        free(*(str2 + i));

        i++;
    }

    free(str1);

    free(str2);

    free(fsize1);

    free(fsize2);

    return 0;
}