#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include <unistd.h>
#include <signal.h>
#include <wait.h>

#include <sys/mman.h>

/* MACROS */
#define PROCESS_COUNT 9
#define MAX_NUMBER_OF_CHILDREN 4
#define MAX_NUMBER_OF_RECIPIENTS 2
#define TEMP_PIDS_FILE "/tmp/pids.txt"
#define USR_GET_LIMIT 101

//#define TIME_SINCE_PROG_START
#define GET_COUNT_IN_FINISH_MESSAGE

/* FUNCTIONS INTERFACE LIST */
void forkFor(int);              // Create all child processes
void die(char *, int);          // Print error message end exit with failure
void reflectPids();             // Reflect all pids in pid list
void wrappedCloseFile();        // Close file and print errors if ones was occured
void waitForInitializing();     // Check errors in one of processes and wait for reflecting pids
void wrappedExit(int);          // Print message and exit
void setUsrAction();            // Set action for handling SIGUSR1/SIGUSR2
void setTermAction();           // Set action for handling SIGTERM
void waitAllChildren();         // Wait all children
void setGroup(int, int);        // Set group of pids
unsigned long getCurrentTime(); // Get current time in microseconds

//void actionLink(int, siginfo_t *, void *);      // SIGUSR1/SIGUSR2 handler
void actionLink1(int sig, siginfo_t *info, void *text);
void actionLink2(int sig, siginfo_t *info, void *text);
void actionLink3(int sig, siginfo_t *info, void *text);
void actionLink4(int sig, siginfo_t *info, void *text);
void actionLink5(int sig, siginfo_t *info, void *text);
void actionLink6(int sig, siginfo_t *info, void *text);
void actionLink7(int sig, siginfo_t *info, void *text);
void actionLink8(int sig, siginfo_t *info, void *text);
void terminateAction(int, siginfo_t *, void *); // SIGTERM handler

/* CONSTANTS */
const int CHILD_NUMBER_OF[PROCESS_COUNT] = {1, 4, 0, 0, 0, 2, 0, 1, 0};

// 1->2 SIGUSR1   2->(3,4) SIGUSR2   4->5 SIGUSR1
// 3->6 SIGUSR1   6->7 SIGUSR1       7->8 SIGUSR2   8->1 SIGUSR2
const void (*actionLink[9])(int, siginfo_t *, void *) = {
    0,
    actionLink1,
    actionLink2,
    actionLink3,
    actionLink4,
    actionLink5,
    actionLink6,
    actionLink7,
    actionLink8

};

const int RECEIVING_SIGNALS[PROCESS_COUNT] = {
    0,       // 0
    SIGUSR2, // 1
    SIGUSR1, // 2
    SIGUSR2, // 3
    SIGUSR2, // 4
    SIGUSR1, // 5
    SIGUSR1, // 6
    SIGUSR1, // 7
    SIGUSR2, // 8
};

const pid_t GROUPS[PROCESS_COUNT] = {
    0, // 0
    1, // 1
    2, // 2
    3, // 3
    3, // 4
    5, // 5
    6, // 6
    7, // 7
    8, // 8
};

// 1->(2,3,4,5)
// 5->(6,7)
// 7->8
const int CHILD_IDS_OF[PROCESS_COUNT][MAX_NUMBER_OF_CHILDREN] = {
    {1},          // 0
    {2, 3, 4, 5}, // 1
    {},           // 2
    {},           // 3
    {},           // 4
    {6, 7},       // 5
    {},           // 6
    {8},          // 7
    {},           // 8
};

/* GLOBAL VARIABLES */
const char *progname;
int usr1Get, usr1Put, usr2Get, usr2Put, selfNumber;
pid_t *pids;
unsigned long startTime;

int main(int argc, char *argv[])
{
    progname = basename(argv[0]);
#ifdef TIME_SINCE_PROG_START
    startTime = 0;
    startTime = getCurrentTime();
#endif
    usr1Get = usr1Put = usr2Get = usr2Put = selfNumber = 0;
    pids = mmap(pids, PROCESS_COUNT * sizeof(pid_t), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    pids[0] = getpid();
    for (int i = 1; i < PROCESS_COUNT; i++)
        pids[i] = 0;

    forkFor(0);
    reflectPids();
    waitForInitializing();

    switch (selfNumber)
    {
    case 0:
        waitpid(pids[1], 0, 0);
        munmap(pids, PROCESS_COUNT * sizeof(pid_t));
        break;
    case 1:
        kill(pids[2], SIGUSR1);
        printf("%d %d %d put USR1 %lu\n", selfNumber, getpid(), getppid(), getCurrentTime());

        while (usr1Get + usr2Get < USR_GET_LIMIT)
            pause();
        ;
        for (int processIndex = 2; processIndex < PROCESS_COUNT; processIndex++)
        {
            kill(pids[processIndex], SIGTERM);
        }
        waitAllChildren();
        wrappedExit(0);
    default:
        while (1)
        {
            pause();
        }
        wrappedExit(0);
    }

    return 0;
}

void setGroup(int pid, int groupPid)
{
    //if (setpgid(getpid(), pids[GROUPS[selfNumber]]) == -1)
    if (setpgid(pid, groupPid) == -1)
    {
        printf("pid: %d group pid %d\n", getpid(), pids[GROUPS[selfNumber]]);
        die("Error during setting group", selfNumber);
    }
}

void waitAllChildren()
{
    while ((wait(0)) > -1)
        ;
}

unsigned long getCurrentTime()
{
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
#ifdef TIME_SINCE_PROG_START
    return (spec.tv_nsec / 1000) % 1000 - startTime;
#else
    return (spec.tv_nsec / 1000) % 1000;
#endif
}

void terminateAction(int sig, siginfo_t *info, void *text)
{
    waitAllChildren();
    wrappedExit(0);
}

void setUsrAction()
{
    if (selfNumber == 0)
        return;

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = actionLink[selfNumber];

    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, RECEIVING_SIGNALS[selfNumber]);
    action.sa_mask = sigset;
    action.sa_flags = SA_SIGINFO;

    sigaction(RECEIVING_SIGNALS[selfNumber], &action, 0);
}

void setTermAction()
{
    if (selfNumber < 2)
        return;

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = terminateAction;

    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGTERM);
    action.sa_mask = sigset;
    action.sa_flags = SA_SIGINFO;

    sigaction(SIGTERM, &action, 0);
}

void waitForInitializing()
{
    char notInitialized;
    do
    {
        notInitialized = 0;
        for (int i = 0; i < PROCESS_COUNT; i++)
        {
            if (pids[i] == -1)
            {
                die("Error occured in one of the processes.", i);
            }
            else if (pids[i] == 0)
            {
                notInitialized = 1;
            }
        }
    } while (notInitialized);
}

void wrappedCloseFile(FILE *file)
{
    if (fclose(file) != 0)
    {
        fprintf(stderr, "%s: pid: %d: Error during closing file. %s\n", progname, getpid(), strerror(errno));
    }
}

void reflectPids()
{
    FILE *tmp = fopen(TEMP_PIDS_FILE, "w");
    if (tmp == NULL)
    {
        die("Can't create temp file", 0);
    }

    for (int i = 0; i < PROCESS_COUNT; i++)
    {
        fprintf(tmp, "%d\n", pids[i]);
    }
    wrappedCloseFile(tmp);
}

void die(char *message, int processIndex)
{
    if (strlen(message) > 0)
    {
        fprintf(stderr, "%s: Error in proccess %d: %s. Strerror: %s\n", progname, processIndex, message, strerror(errno));
    }
    pids[processIndex] = -1;
    wrappedExit(-1);
}

void wrappedExit(int status)
{
#ifdef GET_COUNT_IN_FINISH_MESSAGE
    printf("%d %d %d finished work after putting %3d SIGUSR1 and %3d SIGUSR2, getting %3d SIGUSR1 and %3d SIGUSR2\n", selfNumber, getpid(), getppid(), usr1Put, usr2Put, usr1Get, usr2Get);
#else
    printf("%d %d %d finished work after putting %3d SIGUSR1 and %3d SIGUSR2\n", selfNumber, getpid(), getppid(), usr1Put, usr2Put);
#endif
    munmap(pids, PROCESS_COUNT * sizeof(pid_t));
    exit(status);
}

void forkFor(int parentNum)
{
    if (CHILD_NUMBER_OF[parentNum] == 0)
        return;

    int prevGroup = 0;
    int prevChild = 0;
    for (int processIndex = 0; processIndex < CHILD_NUMBER_OF[parentNum]; processIndex++)
    {
        int childNumber = CHILD_IDS_OF[parentNum][processIndex];
        int pid = fork();
        switch (pid)
        {
        case 0:
            selfNumber = childNumber;
            pids[selfNumber] = getpid();
            setUsrAction();
            setTermAction();
            forkFor(selfNumber);
            return;
        case -1:
            die("Error while forking.", parentNum);
        default:
            if (GROUPS[prevChild] == GROUPS[childNumber])
            {
                setGroup(pid, prevGroup);
            }
            else
            {
                setGroup(pid, pid);
                prevGroup = pid;
                prevChild = childNumber;
            }
        }
    }
}

void actionLink1(int sig, siginfo_t *info, void *text)
{
    printf("%d %d %d get USR2 %lu\n", selfNumber, getpid(), getppid(), getCurrentTime());
    usr2Get++;

    kill(pids[2], SIGUSR1);
    printf("%d %d %d put USR1 %lu\n", selfNumber, getpid(), getppid(), getCurrentTime());
    usr1Put++;
}

void actionLink2(int sig, siginfo_t *info, void *text)
{
    printf("%d %d %d get USR1 %lu\n", selfNumber, getpid(), getppid(), getCurrentTime());
    usr1Get++;

    kill(-pids[3], SIGUSR2);
    printf("%d %d %d put USR2 %lu\n", selfNumber, getpid(), getppid(), getCurrentTime());
    usr2Put++;
}

void actionLink3(int sig, siginfo_t *info, void *text)
{
    printf("%d %d %d get USR2 %lu\n", selfNumber, getpid(), getppid(), getCurrentTime());
    usr2Get++;

    kill(pids[6], SIGUSR1);
    printf("%d %d %d put USR1 %lu\n", selfNumber, getpid(), getppid(), getCurrentTime());
    usr1Put++;
}

void actionLink4(int sig, siginfo_t *info, void *text)
{
    printf("%d %d %d get USR2 %lu\n", selfNumber, getpid(), getppid(), getCurrentTime());
    usr2Get++;

    kill(pids[5], SIGUSR1);
    printf("%d %d %d put USR1 %lu\n", selfNumber, getpid(), getppid(), getCurrentTime());
    usr1Put++;
}

void actionLink5(int sig, siginfo_t *info, void *text)
{
    printf("%d %d %d get USR1 %lu\n", selfNumber, getpid(), getppid(), getCurrentTime());
    usr1Get++;
}

void actionLink6(int sig, siginfo_t *info, void *text)
{
    printf("%d %d %d get USR1 %lu\n", selfNumber, getpid(), getppid(), getCurrentTime());
    usr1Get++;

    kill(pids[7], SIGUSR1);
    printf("%d %d %d put USR1 %lu\n", selfNumber, getpid(), getppid(), getCurrentTime());
    usr1Put++;
}

void actionLink7(int sig, siginfo_t *info, void *text)
{
    printf("%d %d %d get USR2 %lu\n", selfNumber, getpid(), getppid(), getCurrentTime());
    usr2Get++;

    kill(pids[8], SIGUSR2);
    printf("%d %d %d put USR2 %lu\n", selfNumber, getpid(), getppid(), getCurrentTime());
    usr2Put++;
}

void actionLink8(int sig, siginfo_t *info, void *text)
{
    printf("%d %d %d get USR2 %lu\n", selfNumber, getpid(), getppid(), getCurrentTime());
    usr2Get++;

    kill(pids[1], SIGUSR2);
    printf("%d %d %d put USR2 %lu\n", selfNumber, getpid(), getppid(), getCurrentTime());
    usr2Put++;
}
