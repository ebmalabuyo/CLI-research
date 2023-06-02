#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>

#define MAX_COMMAND_LENGTH 200
#define MAX_CHILDREN 10

typedef struct {
    pid_t pid;
    int number;
    int active;
    int pipefd[2];
} ChildProcess;

void executeCommand(const char* command);
void handleFindCommand(const char* command);
void handleListCommand();
void handleKillCommand(const char* command);
void cleanupChildProcesses();
void interruptHandler(int signal);

ChildProcess childProcesses[MAX_CHILDREN];

int main() {
    int i;
    signal(SIGINT, interruptHandler);

    for (i = 0; i < MAX_CHILDREN; i++) {
        childProcesses[i].active = 0;
        pipe(childProcesses[i].pipefd);
    }

    char command[MAX_COMMAND_LENGTH];
    while (1) {
        printf("findstuff$ ");
        fflush(stdout);
        fgets(command, sizeof(command), stdin);

        if (strncmp(command, "find", 4) == 0) {
            handleFindCommand(command);
        } else if (strncmp(command, "list", 4) == 0) {
            handleListCommand();
        } else if (strncmp(command, "kill", 4) == 0) {
            handleKillCommand(command);
        } else if (strncmp(command, "quit", 4) == 0 || strncmp(command, "q", 1) == 0) {
            break;
        } else {
            printf("Invalid command\n");
        }

        cleanupChildProcesses();
    }

    cleanupChildProcesses();
    return 0;
}

void executeCommand(const char* command) {
    FILE* fp;
    char path[1035];

    fp = popen(command, "r");
    if (fp == NULL) {
        printf("Failed to execute command\n");
        return;
    }

    while (fgets(path, sizeof(path), fp) != NULL) {
        printf("%s", path);
    }

    pclose(fp);
}

void handleFindCommand(const char* command) {
    char findCommand[MAX_COMMAND_LENGTH];
    char searchPath[MAX_COMMAND_LENGTH];

    int searchSubdirectories = 0;
    int commandLength = strlen(command);

    if (command[commandLength - 2] == 's') {
        searchSubdirectories = 1;
        strncpy(searchPath, command + 5, commandLength - 7);
    } else {
        strncpy(searchPath, command + 5, commandLength - 6);
    }

    int i;
    for (i = 0; i < MAX_CHILDREN; i++) {
        if (!childProcesses[i].active) {
            childProcesses[i].active = 1;
            break;
        }
    }

    if (i == MAX_CHILDREN) {
        printf("Maximum number of child processes reached\n");
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        close(childProcesses[i].pipefd[0]); /*Close unused read end of the pipe*/

        /*Redirect stdout to the write end of the pipe*/
        dup2(childProcesses[i].pipefd[1], STDOUT_FILENO);
        close(childProcesses[i].pipefd[1]);

        char findCommand[MAX_COMMAND_LENGTH];
        snprintf(findCommand, sizeof(findCommand), "find %s %s", searchPath, searchSubdirectories ? "-s" : "");
        execl("/bin/sh", "sh", "-c", findCommand, NULL);
        exit(0);
    } else if (pid > 0) {
        childProcesses[i].pid = pid;
        childProcesses[i].number = i + 1;
        close(childProcesses[i].pipefd[1]); /*Close unused write end of the pipe*/
    } else {
        printf("Failed to create child process\n");
        childProcesses[i].active = 0;
    }
}

void handleListCommand() {
    int i;
    printf("Child Processes:\n");
    printf("---------------\n");
    for (i = 0; i < MAX_CHILDREN; i++) {
        if (childProcesses[i].active) {
            printf("Child %d: Running\n", childProcesses[i].number);
        }
    }
    printf("---------------\n");
}

void handleKillCommand(const char* command) {
    int processNumber;
    if (sscanf(command, "kill %d", &processNumber) == 1) {
        if (processNumber >= 1 && processNumber <= MAX_CHILDREN) {
            if (childProcesses[processNumber - 1].active) {
                kill(childProcesses[processNumber - 1].pid, SIGKILL);
                close(childProcesses[processNumber - 1].pipefd[0]); /*close pipe*/
                childProcesses[processNumber - 1].active = 0;
                printf("Child %d killed\n", processNumber);
            } else {
                printf("Child %d is not running\n", processNumber);
            }
        } else {
            printf("Invalid process number\n");
        }
    } else {
        printf("Invalid command format\n");
    }
}

void cleanupChildProcesses() {
    int status;
    pid_t pid;
    int i;

    for (i = 0; i < MAX_CHILDREN; i++) {
        if (childProcesses[i].active) {
            pid = waitpid(childProcesses[i].pid, &status, WNOHANG);
            if (pid == childProcesses[i].pid) {
                close(childProcesses[i].pipefd[0]); /*close pipe*/
                childProcesses[i].active = 0;
                printf("Child %d finished\n", childProcesses[i].number);
            }
        }
    }
}

void interruptHandler(int signal) {
    cleanupChildProcesses();
}
