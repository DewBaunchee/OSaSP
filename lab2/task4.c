#include <stdio.h>
#include <stdlib.h>
#include <curses.h>

int isCorrectNumber(char * num) {
	while(*num != 0) {
		if(*num < '0' || *num > '9') return 0;
		num++;
	}
	return 1;
}

int printLines(FILE * file, int n) {
	if(n == 0) n--;
	while(n != 0) {
		int c = getc(file);
		if(c == EOF) return 0;

		printw("%c", c);
		if(c == '\n') n--;
	}
	refresh();
	return 1;
}

int main(int argc, char * argv[]) {
	if(argc < 3) {
		fprintf(stderr, "%s: Error. Count of missing argument: %d\n", argv[0], 3 - argc);
		return -1;
	}

	char hasErrors = 0;
	FILE * file = fopen(argv[1], "r");
	if(file == 0) {
		fprintf(stderr, "%s: Error. Invalid file.\n", argv[0]);
		hasErrors = 1;
	}

	int n = atoi(argv[2]);
	if(!isCorrectNumber(argv[2])) {
		fprintf(stderr, "%s: Error. Invalid number.\n", argv[0]);
		hasErrors = 1;
	}
	if(hasErrors) return -1;

	initscr();
	raw();
	int hasLines = printLines(file, n);
	int c;
	while(hasLines && (c = getch())) {
		if(c == 6) break;
		hasLines = printLines(file, n);
	}

	getch();
	endwin();
	if(fclose(file) != 0) {
		fprintf(stderr, "%s: Error during closing file.\n", argv[0]);
		return -1;
	}
	return 0;
}
