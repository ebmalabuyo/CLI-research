#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <termios.h>
#include <signal.h>

pid_t child_pid;

void sigint_handler(int signum) {
    if (child_pid > 0) {
        kill(child_pid, SIGINT);
    }
}

void sigtstp_handler(int signum) {
    if (child_pid > 0) {
        kill(child_pid, SIGTSTP);
    }
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

void print_prompt(const char *folder) {
    printf("\033[1;34mstat prog\033[0m \033[1m%s$\033[0m ", folder);
    fflush(stdout);
}

void list_directory(const char *folder) {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(folder);
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        printf("%s\n", entry->d_name);
    }

    closedir(dir);
}

void child_process() {
    char input[100];
    char current_folder[PATH_MAX];

    while (1) {
        getcwd(current_folder, sizeof(current_folder));
        print_prompt(current_folder);
        scanf("%s", input);

        if (strcmp(input, "q") == 0) {
            exit(0);
        } else if (strcmp(input, "list") == 0) {
            list_directory(current_folder);
        } else if (strcmp(input, "..") == 0) {
            if (chdir("..") != 0) {
                perror("chdir");
            }
        } else if (input[0] == '/') {
            char new_folder[PATH_MAX];
            snprintf(new_folder, sizeof(new_folder), "%s/%s", current_folder, input + 1);
            if (chdir(new_folder) != 0) {
                perror("chdir");
            }
        } else {
            struct stat file_info;
            if (stat(input, &file_info) == 0) {
                printf("File information:\n");
                printf("Size: %ld bytes\n", file_info.st_size);
                printf("Permissions: %o\n", file_info.st_mode & 0777);
                printf("Owner User ID: %d\n", file_info.st_uid);
                printf("Owner Group ID: %d\n", file_info.st_gid);
            } else {
                printf("File '%s' not found.\n", input);
            }
        }
    }
}

int main() {
    pid_t pid;
    int status;

    setup_signal_handlers();

    while (1) {
        pid = fork();

        if (pid < 0) {
            perror("fork");
            exit(1);
        } else if (pid == 0) {
            // Child process
            child_process();
            exit(0);
        } else {
            // Parent process
            child_pid = pid;
            sleep(10);

            // Check if the child process is still active
            pid_t result = waitpid(pid, &status, WNOHANG);
            if (result == 0) {
                // Child process is still active
                continue;
            } else if (result > 0) {
                // Child process has terminated
                printf("Child process terminated.\n");
                break;
            } else {
                // Error occurred while checking child process
                perror("waitpid");
                break;
            }
        }
    }

    return 0;
}
