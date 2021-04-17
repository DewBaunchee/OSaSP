#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <wait.h>

int msgNum;
unsigned long startTime;

unsigned long getTime()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000 - startTime;
}

void getFatherMsg(int sig, siginfo_t *info, void *text)
{
    printf("Son: %d %d %d get %d %lu\n", msgNum++, getpid(), getppid(), info->si_pid, getTime());
    usleep(100 * 1000);

    printf("Son: %d %d %d put %d %lu\n", msgNum++, getpid(), getppid(), getppid(), getTime());
    kill(getppid(), SIGUSR2);
}

int forking()
{
    int pid = fork();

    switch (pid)
    {
    case 0:
    {
        struct sigaction action;
        memset(&action, 0, sizeof(action));
        action.sa_sigaction = getFatherMsg;

        sigset_t sigset;
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGUSR1);
        action.sa_mask = sigset;
        action.sa_flags = SA_SIGINFO;

        sigaction(SIGUSR1, &action, 0);

        while (1)
            ;
    }
        printf("Son exiting\n");
        exit(0);
    case -1:
        fprintf(stderr, "Error: %s", strerror(errno));
        exit(-1);
    default:
        return pid;
    }
}

void getSonMsg(int sig, siginfo_t *info, void *text)
{
    printf("Father: %d %d %d get %d %lu\n", msgNum++, getpid(), getppid(), info->si_pid, getTime());
}

void solveTask()
{
    startTime = 0;
    startTime = getTime();

    int msgNum = 1;
    int pid1 = forking();
    int pid2 = forking();

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = getSonMsg;

    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR2);
    action.sa_mask = sigset;
    action.sa_flags = SA_SIGINFO;

    sigaction(SIGUSR2, &action, 0);
    signal(SIGUSR1, SIG_IGN);

    int count = 0;
    while (count++ < 40)
    {
        usleep(100 * 1000);

        printf("Father: %d %d %d put %d, %d %lu\n", msgNum++, getpid(), getppid(), pid1, pid2, getTime());
        kill(0, SIGUSR1);
    }

    kill(pid1, SIGKILL);
    kill(pid2, SIGKILL);
    printf("Father exiting\n");
}

int main(int argc, char *argv[])
{
    solveTask();
    return 0;
}