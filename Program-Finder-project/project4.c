#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_CHILDREN 10
#define BUFFER_SIZE 256

typedef struct {
    int num;
    pid_t pid;
    char* filename;
    char* search_text;
    char* file_extension;
    int search_subdirectories;
    int searching;
} ChildProcess;

ChildProcess children[MAX_CHILDREN];
int num_children = 0;

void print_prompt() {
    printf("findstuff$ ");
    fflush(stdout);
}

void add_child(pid_t pid, char* filename, char* search_text, char* file_extension, int search_subdirectories) {
    if (num_children < MAX_CHILDREN) {
        ChildProcess child;
        child.num = num_children + 1;
        child.pid = pid;
        child.filename = strdup(filename);
        child.search_text = strdup(search_text);
        child.file_extension = strdup(file_extension);
        child.search_subdirectories = search_subdirectories;
        child.searching = 1;

        children[num_children++] = child;
    } else {
        printf("Maximum number of children reached.\n");
    }
}

void remove_child(int num) {
    int i;
    for (i = 0; i < num_children; i++) {
        if (children[i].num == num) {
            free(children[i].filename);
            free(children[i].search_text);
            free(children[i].file_extension);
            memmove(&children[i], &children[i+1], (num_children - i - 1) * sizeof(ChildProcess));
            num_children--;
            break;
        }
    }
}

void list_children() {
    int i;
    for (i = 0; i < num_children; i++) {
        printf("Child %d (PID: %d): Searching for '%s' in %s files", children[i].num, children[i].pid, children[i].search_text, children[i].file_extension);
        if (children[i].search_subdirectories)
            printf(" (including subdirectories)");
        printf("\n");
    }
}

void kill_child(int num) {
    int i;
    for (i = 0; i < num_children; i++) {
        if (children[i].num == num) {
            kill(children[i].pid, SIGTERM);
            break;
        }
    }
}

void cleanup_children() {
    int i;
    for (i = 0; i < num_children; i++) {
        kill(children[i].pid, SIGTERM);
    }
}

void handle_signal(int signum) {
    if (signum == SIGCHLD) {
        pid_t pid;
        int status;

        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            int i;
            for (i = 0; i < num_children; i++) {
                if (children[i].pid == pid) {
                    children[i].searching = 0;
                    printf("Child %d (PID: %d): Search completed.\n", children[i].num, children[i].pid);
                    break;
                }
            }
        }
    }
}

void execute_find(char* filename, char* search_text, char* file_extension, int search_subdirectories) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        printf("Failed to create pipe.\n");
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        close(pipefd[0]);  // Close the read end of the pipe

        // Redirect stdout to the write end of the pipe
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        char command[BUFFER_SIZE];
        snprintf(command, BUFFER_SIZE, "find . %s -name \"%s\" %s -print", search_subdirectories ? "-type f" : "", filename, file_extension);

        // Execute the find command
        system(command);

        exit(0);
    } else if (pid > 0) {
        // Parent process
        close(pipefd[1]);  // Close the write end of the pipe

        add_child(pid, filename, search_text, file_extension, search_subdirectories);

        // Read and print the output from the child process
        char buffer[BUFFER_SIZE];
        ssize_t bytes_read;
        while ((bytes_read = read(pipefd[0], buffer, BUFFER_SIZE)) > 0) {
            write(STDOUT_FILENO, buffer, bytes_read);
        }

        close(pipefd[0]);
    } else {
        printf("Failed to fork child process.\n");
    }
}

int main() {
    signal(SIGCHLD, handle_signal);

    char input[BUFFER_SIZE];
    while (1) {
        print_prompt();

        if (fgets(input, BUFFER_SIZE, stdin) == NULL) {
            printf("\n");
            break;
        }

        input[strcspn(input, "\n")] = '\0';  // Remove trailing newline

        char* command = strtok(input, " ");
        if (command == NULL)
            continue;

        if (strcmp(command, "find") == 0) {
            char* filename = strtok(NULL, " ");
            if (filename == NULL)
                continue;

            char* file_extension = NULL;
            int search_subdirectories = 0;

            // Check if -f flag is set
            char* flag = strtok(NULL, " ");
            if (flag != NULL && strcmp(flag, "-f") == 0) {
                file_extension = strtok(NULL, " ");
                if (file_extension == NULL)
                    continue;

                flag = strtok(NULL, " ");
            }

            // Check if -s flag is set
            if (flag != NULL && strcmp(flag, "-s") == 0) {
                search_subdirectories = 1;
            }

            execute_find(filename, filename, file_extension, search_subdirectories);
        } else if (strcmp(command, "list") == 0) {
            list_children();
        } else if (strcmp(command, "kill") == 0) {
            char* num_str = strtok(NULL, " ");
            if (num_str == NULL)
                continue;

            int num = atoi(num_str);
            kill_child(num);
        } else if (strcmp(command, "quit") == 0 || strcmp(command, "q") == 0) {
            cleanup_children();
            break;
        }
    }

    return 0;
}
