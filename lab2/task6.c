#include <stdio.h>
#include <dirent.h>
#include <sys/dir.h>

void printContent(DIR * dir) {
	struct dirent * content;
	while((content = readdir(dir)) != NULL) {
		printf("%s\n", content->d_name);
	}
}

int main() {
	DIR * root = opendir("/");
	char currentPath[128];
	getcwd(currentPath, 128);
	DIR * current = opendir(currentPath);

	printContent(root);
	printContent(current);

	closedir(root);
	closedir(current);
	return 0;
}
