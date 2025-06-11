#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_CMD 1024
#define MAX_ARGS 100
#define HISTORY_SIZE 100

char *history[HISTORY_SIZE];
int history_count = 0;

// Adiciona ao histórico
void add_to_history(char *cmd) {
    if (history_count == HISTORY_SIZE) {
        free(history[0]);
        memmove(history, history + 1, sizeof(char *) * (HISTORY_SIZE - 1));
        history_count--;
    }
    history[history_count++] = strdup(cmd);
}

// Mostra histórico
void print_history() {
    for (int i = 0; i < history_count; i++)
        printf("%d: %s\n", i + 1, history[i]);
}

// Divide entrada em tokens
char **parse_input(char *input, int *background, int *in_redir, int *out_redir, char **in_file, char **out_file, int *append, int *has_pipe) {
    char **args = malloc(sizeof(char *) * MAX_ARGS);
    int i = 0;

    char *token = strtok(input, " \t\n");
    while (token) {
        if (strcmp(token, "&") == 0) {
            *background = 1;
        } else if (strcmp(token, "<") == 0) {
            *in_redir = 1;
            token = strtok(NULL, " \t\n");
            *in_file = token;
        } else if (strcmp(token, ">>") == 0) {
            *append = 1;
            *out_redir = 1;
            token = strtok(NULL, " \t\n");
            *out_file = token;
        } else if (strcmp(token, ">") == 0) {
            *out_redir = 1;
            token = strtok(NULL, " \t\n");
            *out_file = token;
        } else if (strcmp(token, "|") == 0) {
            *has_pipe = 1;
            args[i++] = NULL;
            token = strtok(NULL, "");
            args[i++] = token;
            break;
        } else {
            args[i++] = token;
        }
        token = strtok(NULL, " \t\n");
    }

    args[i] = NULL;
    return args;
}

// Executa comando (com ou sem pipe)
void execute_command(char **args, int background, int in_redir, int out_redir, char *in_file, char *out_file, int append, int has_pipe) {
    int pipefd[2];

    if (has_pipe) pipe(pipefd);

    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("Erro ao criar processo");
        return;
    }

    if (pid1 == 0) { // filho 1
        if (has_pipe) dup2(pipefd[1], STDOUT_FILENO);
        if (in_redir) {
            int fd = open(in_file, O_RDONLY);
            if (fd < 0) { perror("Erro na entrada"); exit(1); }
            dup2(fd, STDIN_FILENO); close(fd);
        }
        if (!has_pipe && out_redir) {
            int fd = open(out_file, (append ? O_WRONLY|O_CREAT|O_APPEND : O_WRONLY|O_CREAT|O_TRUNC), 0644);
            if (fd < 0) { perror("Erro na saída"); exit(1); }
            dup2(fd, STDOUT_FILENO); close(fd);
        }
        if (has_pipe) close(pipefd[0]);
        execvp(args[0], args);
        perror("Comando não encontrado");
        exit(1);
    }

    if (has_pipe) {
        pid_t pid2 = fork();
        if (pid2 < 0) {
            perror("Erro ao criar segundo processo");
            return;
        }

        if (pid2 == 0) { // filho 2
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[1]);

            char *pipe_cmd = args[1];
            char *pipe_args[] = {pipe_cmd, NULL};
            execvp(pipe_cmd, pipe_args);
            perror("Erro no pipe");
            exit(1);
        }
        close(pipefd[0]);
        close(pipefd[1]);
    }

    if (!background) wait(NULL);
    if (has_pipe) wait(NULL);
}

// Loop principal da shell
int main() {
    char input[MAX_CMD];

    while (1) {
        printf("Minha_Shell> ");
        fflush(stdout);

        if (!fgets(input, MAX_CMD, stdin)) break;
        if (strcmp(input, "\n") == 0) continue;

        input[strcspn(input, "\n")] = '\0'; // remove newline
        add_to_history(input);

        if (strcmp(input, "exit") == 0) break;
        if (strcmp(input, "history") == 0) {
            print_history();
            continue;
        }

        int bg = 0, in_redir = 0, out_redir = 0, append = 0, has_pipe = 0;
        char *in_file = NULL, *out_file = NULL;

        char **args = parse_input(input, &bg, &in_redir, &out_redir, &in_file, &out_file, &append, &has_pipe);
        if (args[0] == NULL) continue;

        execute_command(args, bg, in_redir, out_redir, in_file, out_file, append, has_pipe);
        free(args);
    }

    // libera histórico
    for (int i = 0; i < history_count; i++) free(history[i]);

    return 0;
}
