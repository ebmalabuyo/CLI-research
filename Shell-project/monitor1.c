#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/select.h>

#define MAX_PATH_LENGTH 256
#define TIMEOUT_SECONDS 10

time_t last_input_time = 0;
pid_t child_pid;
int should_exit_child = 0;

void sigint_handler(int signum) {
    // Ignore SIGINT in the child process
}

void sigtstp_handler(int signum) {
    // Ignore SIGTSTP in the child process
}

void setup_signal_handlers() {
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = sigtstp_handler;
    sigaction(SIGTSTP, &sa, NULL);
}

void child_process() {
    setup_signal_handlers();

    char input[MAX_PATH_LENGTH];

    while (!should_exit_child) {
        printf("\033[0;34mProgram 3 Monitor1%s$\033[0m ", getcwd(NULL, 0));
        fflush(stdout);
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = '\0'; /*helps to get rid of trailing whitespace*/

        last_input_time = time(NULL);

        if (strcmp(input, "q") == 0) {
            exit(0);
        } else if (strcmp(input, "list") == 0) {
            DIR *dir;
            struct dirent *entry;

            dir = opendir(".");
            if (dir == NULL) {
                perror("opendir");
                exit(1);
            }

            while ((entry = readdir(dir)) != NULL) {
                printf("%s\n", entry->d_name);
            }

            closedir(dir);
        } else if (strcmp(input, "..") == 0) {
            chdir("..");
        } else if (input[0] == '/') {
            if (chdir(input + 1) != 0) {
                printf("Folder does not exist.\n");
            }
        } else {
            struct stat file_stat;

            if (stat(input, &file_stat) == 0) {
                printf("File Information:\n");
                printf("Size: %ld bytes\n", file_stat.st_size);
                printf("Permissions: %o\n", file_stat.st_mode & 0777);
                printf("Owner ID: %d\n", file_stat.st_uid);
                printf("Group ID: %d\n", file_stat.st_gid);
            } else {
                printf("File not found.\n");
            }
        }
    }
}

void parent_process(pid_t child_pid) {
    fd_set read_fds;
    struct timeval timeout;
    int retval;

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);

        timeout.tv_sec = TIMEOUT_SECONDS;
        timeout.tv_usec = 0;

        retval = select(STDIN_FILENO + 1, &read_fds, NULL, NULL, &timeout);

        if (retval == -1) {
            perror("select");
            exit(1);
        } else if (retval == 0) {
            // Timeout, fork a new child process
            pid_t new_pid = fork();

            if (new_pid == -1) {
                perror("fork");
                exit(1);
            } else if (new_pid == 0) {
                // Child process
                child_pid = getpid();  // Set the child_pid variable in the child process
                child_process();
            } else {
                // Parent process
                should_exit_child = 1;  // Set the flag to exit the previous child process
                child_pid = new_pid;    // Update child_pid to the new child process
            }
        } else {
            if (FD_ISSET(STDIN_FILENO, &read_fds)) {
                // Input received, continue waiting
                continue;
            }
        }
    }
}

int main() {
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    } else if (pid == 0) {
        // Child process
        child_pid = getpid();  // Set the child_pid variable in the child process
        child_process();
    } else {
        // Parent process
        should_exit_child = 1;  // Set the flag to exit the initial child process
        child_pid = pid;
        parent_process(pid);
    }

    return 0;
}
