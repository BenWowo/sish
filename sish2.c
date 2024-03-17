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
#define READ 0
#define WRITE 1
typedef int8_t bool;

struct HistoryListNode {
    char* line;
    struct HistoryListNode *next;
};

typedef struct HistoryListNode HistoryListNode;

struct HistoryLinkedList{
    HistoryListNode *head;
    HistoryListNode *tail;
    int capacity;    
    int size;
};
typedef struct HistoryLinkedList HistoryLinkedList;

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
void execCmd(CMD *cmd);
void execCmds(CMD **cmds);
void history_add(HistoryLinkedList *history, char *input);
void history_clear(HistoryLinkedList *history);
void history_get(HistoryLinkedList *history, int offset);
void history_init(HistoryLinkedList *history);
void freeHistory(HistoryLinkedList *history);

// utility functions
void exit_err(char *msg);
void make_fork(pid_t *cpid, char *msg);
void make_pipe(int **pipe_fd, char *msg);
void make_dup2(int fd1, int fd2, char *msg);

HistoryLinkedList history = {0}; // this is bad practice...

int 
main(int argc, char* argv[]) {
    history_init(&history);

    while(true) {
        const char* prompt = PROMPT;
        printf("%s", prompt);

        char* input = getInput();
        // printf("input: %s\n", input);
        // history_add(&history, input);

        char** tokens = getTokens(input);
        // printTokens(tokens);

        CMD** cmds = getCmds(tokens);
        printCmds(cmds);

        // maybe I should do error validation here?

        execCmds(cmds);

        freeCmds(cmds);
        free(tokens);
        free(input);
    }
    freeHistory(&history);    

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
execCmd(CMD* cmd) {
    printf("EXEC cmd is being called right now");

    if (strcmp(cmd->args[0], "exit") == 0) {
        printf("This is the exit command being executed");
    } else if (strcmp(cmd->args[0], "cd") == 0) {
        printf("This is the firs thing in the thing");
    } else if (strcmp(cmd->args[0], "history") == 0) {
        // check for clear and check for offset
        // bool shouldClear = false;
        // int offset = 0

        // history_get(&history, offset);

        // history_clear(&history);
    } else {
        printf("This is the last case in the execCmd block\n");
        execvp(cmd->args[0], cmd->args);
        exit_err("failed to execvp");
    }
    return;
    // to launch an executable
    // just do execvp the name of whatever they typed
    // and make sure to make null the last arg of execvp
}

// This doesn't work for some reason
void
execCmds(CMD **cmds) {
    CMD* childrenCmds[BUFFERSIZE];
    int numChildren = 0;
    int numPipes = 0;
    for (int cmdIndex = 0; cmds[cmdIndex] != NULL; cmdIndex++) {
        if (cmds[cmdIndex]->isPipe == true) {
            numPipes++;
        } else {
            childrenCmds[numChildren] = cmds[cmdIndex];
            numChildren++;
        }
    }
    int **pipes = (int**)malloc(sizeof(int*) * numPipes);
    pid_t *cpids = (pid_t*)malloc(sizeof(pid_t) * numChildren);

    for (int pipeIndex = 0; pipeIndex < numPipes; pipeIndex++) {
       make_pipe(&pipes[pipeIndex], "failed to make pipe"); 
    }

    for (int childIndex = 0; childIndex < numChildren; childIndex++) {
        CMD* childCmd = childrenCmds[childIndex];
        make_fork(&cpids[childIndex], "failed to make fork");
        if (cpids[childIndex] == 0) {
            // close all pipes that it is not using
            // close all pipes execpt fd[n-1] and fd[n]

            // cpid 0 | reads from stdin, writes to fd[0]
            // cpid 1 | reads from fd[0], writes to fd[1]
            // ...
            // cpid n | reads from fd[n-1], writes to stdout
            for (int pipeIndex = 0; pipeIndex < numPipes; pipeIndex++) {
                if (pipeIndex != childIndex-1 && pipeIndex != childIndex) {
                    for (int pipeEnd = 0; pipeEnd < 2; pipeEnd++) {
                        close(pipes[pipeIndex][pipeEnd]);
                    }
                }
            }
            printf("Is it getting stuck anywhere???");

            if (childIndex == 0 && childIndex == numChildren - 1) { // one child case
                printf("One child CASE!\n");
                execCmd(childCmd);
            } else if (childIndex == 0) {
                printf("This is the first of many children\n");
                close(pipes[0][READ]); // not reading from pipe to next process

                make_dup2(pipes[0][WRITE], STDOUT_FILENO, "failed to make dup2");
                close(pipes[0][WRITE]); // close write end bc dup2

                execCmd(childCmd);
            } else if (childIndex == numChildren - 1) {
                printf("This is the last of many children\n");
                close(pipes[childIndex-1][WRITE]); // not writing to pipe from prev process
                make_dup2(pipes[childIndex-1][READ], STDIN_FILENO, "failed to make dup2");
                close(pipes[childIndex-1][READ]); // close read end bc dup2

                execCmd(childCmd);
            } else {
                printf("This is one of many children\n");
                close(pipes[childIndex-1][WRITE]); // not writing to pipe from prev process
                close(pipes[childIndex][READ]);    // not reading from pipe to next process

                make_dup2(pipes[childIndex-1][READ], STDIN_FILENO, "failed to make dup2");
                make_dup2(pipes[childIndex][WRITE], STDOUT_FILENO, "failed to make dup2");

                close(pipes[childIndex-1][READ]); // close read end bc dup2
                close(pipes[childIndex][WRITE]);  // close write end bc dup2

                execCmd(childCmd);
            }
            return;
        }
    }


    // close all of the pipes in main process because it doesn't use them
    for (int i = 0; i < numPipes; i++) {
        for (int pipeEnd = 0; pipeEnd < 2; pipeEnd++) {
            close(pipes[i][pipeEnd]);
        }
    }

    // wait for all of the children processes to finish
    for (int i = 0; i < numChildren; i++) {
        waitpid(cpids[i], NULL, 0);
    }

    free(cpids);
    for (int i = 0; i < numPipes; i++) {
        free(pipes[i]);
    }
    free(pipes);

    printf("How many processes get to this part\n");
}

void
history_add(HistoryLinkedList *history, char *input) {
    // edge case for empty history
    // history of size 1
    // history of 

    HistoryListNode* newTail = (HistoryListNode*)malloc(sizeof(HistoryListNode));
    newTail->line = input;
    newTail->next = NULL;
    history->tail->next = newTail;
    history->tail = newTail;

    if (history->size == history->capacity) {
        
    } 
}

void
history_clear(HistoryLinkedList *history) {

}

void
history_print(HistoryLinkedList *history, int offset) {
    // print the history starting from the offset if it is in bounds

}

void
history_init(HistoryLinkedList *history) {
    history->head = NULL;
    history->tail = NULL;
    history->size = 0;
    history->capacity = 100;
}
void
freeHistory(HistoryLinkedList *history) {
    // free all of the linked list nodes
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