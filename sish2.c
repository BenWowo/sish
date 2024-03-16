#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#define BUFFERSIZE 1028
#define MAX_NUM_TOKENS 1028
#define MAX_NUM_CMDS 1028
#define MAX_NUM_FLAGS 1028
#define PROMPT "sish> "
#define true 1
#define false 0
typedef int8_t bool;

struct CMD {
    char** args;
    bool isPipe;
};
typedef struct CMD CMD;

char *getInput();
char **getTokens(char *input);
void printTokens(char **tokens);
CMD  **getCmds(char **tokens);
void freeCmds(CMD **cmds);
void printCmds(CMD **cmds);
void execCmds(CMD **cmds);

// utility functions
void exit_err(char *msg);
void make_fork(pid_t *cpid, char *msg);
void make_pipe(int **pipe_fd, char *msg);
void make_dup2(int fd1, int fd2, char *msg);

int 
main(int argc, char* argv[]) {
    while(true) {
        const char* prompt = PROMPT;
        printf("%s", prompt);

        char* input = getInput();
        // printf("input: %s\n", input);

        char** tokens = getTokens(input);
        // printTokens(tokens);

        CMD** cmds = getCmds(tokens);
        // printCmds(cmds);

        // maybe I should do error validation here?

        execCmds(cmds);

        freeCmds(cmds);
        free(tokens);
        free(input);
    }
    printf("Thank you for using sish!\n");
    return EXIT_SUCCESS;
}

char *
getInput() {
    size_t bufferSize = BUFFERSIZE;
    char *line = (char*)malloc(sizeof(char) * bufferSize);
    ssize_t bytesRead;
    if ((bytesRead = getline(&line, &bufferSize, stdin)) == -1) {
        printf("This is a non fatal error!\n");
    }
    line[bytesRead - 1] = '\0';
    return line;
}

char **
getTokens(char* input) {
    int tokenIndex = 0;
    char** tokens = (char**)malloc(sizeof(char*) * MAX_NUM_TOKENS);
    char *token, *position;
    char *delimiters = " ";

    token = strtok_r(input, delimiters, &position);
    tokens[tokenIndex] = token;
    tokenIndex++;
    while((token = strtok_r(NULL, delimiters, &position)) != NULL) {
        tokens[tokenIndex] = token;
        tokenIndex++;
    }
    
    tokens[tokenIndex] = NULL;
    return tokens;
}

void
printTokens(char **tokens) {
    for (int tokenIndex = 0; tokens[tokenIndex] != NULL; tokenIndex++) {
        printf("token: %s\n", tokens[tokenIndex]);
    }
}



// I'm thinking of writing a BNF grammer for commands
// I have to use CMD** instead of just CMD* because the cmds are dynamically allocated
CMD **
getCmds(char **tokens) {
    CMD** cmds = (CMD**)malloc(sizeof(CMD*) * MAX_NUM_CMDS);
    int tokenIndex = 0;
    int cmdIndex = 0;
    while (tokens[tokenIndex] != NULL) {
        CMD* cmd = (CMD*)malloc(sizeof(CMD));
        cmd->args = (char**)malloc(sizeof(char*) * MAX_NUM_FLAGS);
        int argIndex = 0;

        if (strcmp(tokens[tokenIndex], "|") == 0) {
            cmd->args[argIndex] = tokens[tokenIndex];
            cmd->isPipe = true;
            argIndex++;
            tokenIndex++;
        } else {
            // keep looking ahead until pipe or the end of tokens
            cmd->isPipe = false;
            while (tokens[tokenIndex] != NULL && strcmp(tokens[tokenIndex], "|") != 0) {
                cmd->args[argIndex] = tokens[tokenIndex];
                argIndex++;
                tokenIndex++;
            }
        }

        cmd->args[argIndex] = NULL;
        cmds[cmdIndex] = cmd;
        cmdIndex++;
    }
    cmds[cmdIndex] = NULL;
    return cmds;
}

void
freeCmds(CMD** cmds) {
    for (int cmdIndex = 0; cmds[cmdIndex] != NULL; cmdIndex++) {
        free(cmds[cmdIndex]->args);
    }
    free(cmds);
}

void
printCmds(CMD** cmds) {
    for (int cmdIndex = 0; cmds[cmdIndex] != NULL; cmdIndex++) {
        printf("cmd: ");
        for (int argIndex = 0; cmds[cmdIndex]->args[argIndex] != NULL; argIndex++) {
            printf("%s ", cmds[cmdIndex]->args[argIndex]);
        }
        printf("\n");
    }
}

void
execCmds(CMD **cmds) {
    int numChildren = 0;
    int numPipes = 0;
    for (int cmdIndex = 0; cmds[cmdIndex] != NULL; cmdIndex++) {
        if (cmds[cmdIndex]->isPipe == true) {
            numPipes++;
        } else {
            numChildren++;
        }
    }
    int **fds = malloc(sizeof(int*) * numPipes);
    pid_t *cpids = (pid_t*)malloc(sizeof(pid_t) * numChildren);

    for (int i = 0; i < numPipes; i++) {
       make_pipe(&fds[i], "failed to make pipe"); 
    }

    // execute and pipe the commands together here
    for (int i = 0; i < numChildren; i++) {
        printf("how many things are inside of this loop???\n");

        // wait I think my pointer arithmatic is off and it is not making the forks
        make_fork(&cpids[i], "failed to make fork");
        if (cpids[i] == 0) {
            // close all pipes that it is not using
            // close all pipes execpt fd[n-1] and fd[n]

            // cpid 0 | reads from stdin, writes to fd[0]
            // cpid 1 | reads from fd[0], writes to fd[1]
            // ...
            // cpid n | reads from fd[n-1], writes to stdout
            for (int pipeIndex = 0; pipeIndex < numPipes; pipeIndex) {
                for (int pipeEnd = 0; pipeEnd < 2; pipeEnd++) {
                    if (pipeIndex != i-1 && pipeIndex != i) {
                        close(fds[pipeIndex][pipeEnd]);
                    }
                }
            }

            if (i == 0 && i == numChildren - 1) { // one child case
                // just execute the command
                execvp(cmds[i], cmd[i]->args);
                exit_err("failed to execvp");
            } else if (i == 0) {
                close(fds[0][0]); // not reading from pipe to next process

                make_dup2(fds[0][1], STDOUT_FILENO, "failed to make dup2");
                close(fds[0][1]); // close write end bc dup2

                // execvp(cmds[i]->args[0], cmds[i]);
                // exit_err("execvp failed");
            } else if (i == numChildren - 1) {
                close(fds[i-1][1]); // not writing to pipe from prev process
                close(fds[i][0]);   // not reading from pipe to next proccess

                make_dup2(fds[i][0], STDIN_FILENO, "failed to make dup2");
                close(fds[i][0]);   // close write end of pipe bc dup2

                // execvp(cmds[i]->args[0], cmds[i]);
                // exit_err("execvp failed");
            } else {
                close(fds[i-1][1]); // not writing to pipe from prev process
                close(fds[i][0]);   // not reading from pipe to next process

                make_dup2(fds[i-1], STDIN_FILENO, "failed to make dup2");
                make_dup2(fds[i], STDOUT_FILENO, "failed to make dup2");

                close(fds[i-1][0]);
                close(fds[i][1]);

                // execvp(cmds[i]->args[0], cmds[i]);
                // exit_err("execvp failed");
            }

            // to launch an executable
            // just do execvp the name of whatever they typed
            // and make sure to make null the last arg of execvp

            printf("This is the child\n");
            return;
        }
    }
    

    printf("How many processes get to this part");


    // close all of the pipes in main process because it doesn't use them
    for (int i = 0; i < numPipes; i++) {
        for (int pipeEnd = 0; pipeEnd < 2; pipeEnd++) {
            close(fds[i][pipeEnd]);
        }
    }

    // wait for all of the children processes to finish
    for (int i = 0; i < numChildren; i++) {
        waitpid(cpids[i], NULL, 0);
    }

}

void
exit_err(char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void
make_fork(pid_t* cpid, char* msg) {
    if ((*cpid = fork()) < 0) {
        exit_err(msg);
    }
}

void
make_pipe(int** pipe_fd, char* msg) {
    *pipe_fd = (int*)malloc(2 * sizeof(int));
    if (pipe(*pipe_fd) == - 1) {
        exit_err(msg);
    }
}

void
make_dup2(int fd1, int fd2, char* msg) {
    if ((dup2(fd1, fd2)) == -1) {
        exit_err(msg);
    }
}