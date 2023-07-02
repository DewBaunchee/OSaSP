#include <stdio.h>

int main(int argc, char * argv[]) {
	if(argc < 2) {
		fprintf(stderr, "%s: Missing argument (enter path to file).\n", argv[0]);
		return -1;
	}

	FILE * file = fopen(argv[1], "w");
	if(file == 0) {
		fprintf(stderr, "%s: Invalid file.\n", argv[0]);
		return -1;
	}

	int c;
	while((c = getc(stdin)) != EOF) {
		fputc(c, file);
	}

	if(fclose(file) != 0) {
		fprintf(stderr, "%s: Error during closing file.\n", argv[0]);
		return -1;
	}
	return 0;
}
