#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int isCorrectNumber(char* num) {
    for(int i = 0; i < strlen(num); i++) {
        if(num[i] > '9' || num[i] < '0') return 0;
    }
    return 1;
}

int lastIndexOf(char * string, char symbol) {
	int i  = strlen(string) - 1;
	while(string[i] != symbol) i--;
	return i;
}

int isCorrectPhoneNumber(char* num) {}

int main(int argc, char **argv) {
    int hasErrors = 0;
    if(argc != 3) {
	fprintf(stderr, "%s: Error. This program require 2 arguments. Count of missing arguments: %d\n", argv[0] + lastIndexOf(argv[0], '/') + 1, 3 - argc);
	return -1;
    }

    if(!isCorrectNumber(argv[1])) {
        fprintf(stderr, "%s: Error. Argument #1: invalid symbols.\n", argv[0] + lastIndexOf(argv[0], '/') + 1);
    	hasErrors = 1;
    }

    if(!isCorrectNumber(argv[2])) {
        fprintf(stderr, "%s: Error. Argument #2: invalid symbols.\n", argv[0] + lastIndexOf(argv[0], '/') + 1);
        hasErrors = 1;
    }

    if(strlen(argv[1]) != 7) {
	fprintf(stderr, "%s: Error. Wrong phone number format\n", argv[0] + lastIndexOf(argv[0], '/') + 1);
	hasErrors = 1;
    }

    if(hasErrors) return -1;

    int number = atoi(argv[1]);
    int k = atoi(argv[2]);

    int result = number % k + 1;

    printf("%d\n", result);
}
