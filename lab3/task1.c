#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

void solveTask() {
    char * buf = calloc(128, sizeof(char));
    struct timeval time;
    gettimeofday(&time, NULL);
    strftime(buf, 128, "%H:%M:%S", localtime(&time.tv_sec));
    printf("My pid is %d\nPid of my parent is %d\nTime is %s:%ld\n", getpid(), getppid(), buf, time.tv_usec % 1000);
    system("ps -x");
    free(buf);
}

int main() {
    pid_t child1, child2;
    child1 = fork();
    if(child1 > 0) {
        child2 = fork();
    }
    
    solveTask();

    return 0;   
}