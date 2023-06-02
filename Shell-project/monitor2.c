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

#define MAX_CHILDREN 10

pid_t child_pid;

void sigint_handler(int signum) {
    printf("Nice try\n");
}

void sigtstp_handler(int signum) {
    printf("Nice try\n");
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
            if (chdir(input) != 0) {
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
    int children_count = 0;

    setup_signal_handlers();

    while (1) {
        if (children_count >= MAX_CHILDREN) {
            wait(NULL);  
            children_count--;
        }

        pid = fork();

        if (pid < 0) {
            perror("fork");
            exit(1);
        } else if (pid == 0) {
            /*Child process*/
            child_process();
            exit(0);
        } else {
            /*Parent process*/
            child_pid = pid;
            children_count++;
            waitpid(pid, &status, 0);
            sleep(10);
        }
    }

    return 0;
}
