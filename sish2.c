#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <ctype.h>

#define BUFFERSIZE 4096
#define MAX_NUM_TOKENS 4096
#define MAX_NUM_CMDS 4096
#define MAX_NUM_ARGS 4096
#define PROMPT "sish> "
#define true 1
#define false 0
#define READ 0
#define WRITE 1
typedef int8_t bool;

struct HistoryListNode {
    char line[BUFFERSIZE];
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
char *history_get(HistoryLinkedList *history, int offset);
void history_init(HistoryLinkedList *history);
void history_print(HistoryLinkedList *history);
void freeHistory(HistoryLinkedList *history);

// utility functions
void exit_err(char *msg);
void make_fork(pid_t *cpid, char *msg);
void make_pipe(int **pipe_fd, char *msg);
void make_dup2(int fd1, int fd2, char *msg);
bool isInteger(char* number);
bool didEarlyExit = false;

HistoryLinkedList history = {0}; // this is bad practice...

int
main(int argc, char* argv[]) {
    history_init(&history);

    while(true) {
        const char* prompt = PROMPT;
        printf("%s", prompt);

        char* input = getInput();
        //printf("input: %s\n", input);

        history_add(&history, input);

        char** tokens = getTokens(input);
        // printTokens(tokens);

        CMD** cmds = getCmds(tokens);
        // printCmds(cmds);

        // I guess I can do error validation inside of execCmds
        execCmds(cmds);

        freeCmds(cmds);
        free(tokens);
        free(input);

        if(didEarlyExit){
            break;
        }
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

CMD **
getCmds(char **tokens) {
    CMD** cmds = (CMD**)malloc(sizeof(CMD*) * MAX_NUM_CMDS);
    int tokenIndex = 0;
    int cmdIndex = 0;
    while (tokens[tokenIndex] != NULL) {
        CMD* cmd = (CMD*)malloc(sizeof(CMD));
        cmd->args = (char**)malloc(sizeof(char*) * MAX_NUM_ARGS);
        int argIndex = 0;

        if (strcmp(tokens[tokenIndex], "|") == 0) {
            cmd->args[argIndex] = tokens[tokenIndex];
            cmd->isPipe = true;
            argIndex++;
            tokenIndex++;
        } else {
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

    execvp(cmd->args[0], cmd->args);
    printf(cmd->args[0], cmd->args[1]);
    exit_err("failed to execvp\t");

    return;
}

void
execCmds(CMD **cmds) {
    // I guess I can validate the commands in here???????

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
        if (strcmp(childCmd->args[0], "exit") == 0) {
            didEarlyExit = true;
            return;
        } else if (strcmp(childCmd->args[0], "cd") == 0) {
            int dir = chdir(childCmd->args[1]);
            if(dir != 0){
                printf("Failed to change directory to %s", childCmd -> args[1]);
            }
        } else if (strcmp(childCmd->args[0], "history") == 0) {
            // check for clear and check for offset
            bool shouldClear = false;
            int offset = -1;
            for (int argIndex = 1; childCmd->args[argIndex] != NULL; argIndex++) {
                if (strcmp(childCmd->args[argIndex], "-c") == 0) {
                    shouldClear = true;
                } else if (isInteger(childCmd->args[argIndex])) {
                    offset = atoi(childCmd->args[argIndex]);
                } else {
                    printf("bad arguments supplied to history cmd\n");
                    //exit(0);
                }
            }

            if (shouldClear) {
                history_clear(&history);
            } else if (offset != -1) {
                // execute cmd at offset
                char* line = history_get(&history, offset);
                history_add(&history, line);
                CMD **cmds = getCmds(&line);
                execCmds(cmds);
            }
            else{
                history_print(&history);
            }

        } else {
            make_fork(&cpids[childIndex], "failed to make fork");
            if (cpids[childIndex] == 0) {
                // close all pipes that it is not using
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

                if (childIndex == 0 && childIndex == numChildren - 1) {
                    // one child case
                    execCmd(childCmd);
                } else if (childIndex == 0) {
                    close(pipes[0][READ]); // not reading from pipe to next process
                    make_dup2(pipes[0][WRITE], STDOUT_FILENO, "failed to make dup2");
                    close(pipes[0][WRITE]); // close write end bc dup2

                    execCmd(childCmd);
                } else if (childIndex == numChildren - 1) {
                    close(pipes[childIndex-1][WRITE]); // not writing to pipe from prev process
                    make_dup2(pipes[childIndex-1][READ], STDIN_FILENO, "failed to make dup2");
                    close(pipes[childIndex-1][READ]);  // close read end bc dup2

                    execCmd(childCmd);
                } else {
                    close(pipes[childIndex-1][WRITE]); // not writing to pipe from prev process
                    close(pipes[childIndex][READ]);    // not reading from pipe to next process

                    make_dup2(pipes[childIndex-1][READ], STDIN_FILENO, "failed to make dup2");
                    make_dup2(pipes[childIndex][WRITE], STDOUT_FILENO, "failed to make dup2");

                    close(pipes[childIndex-1][READ]); // close read end bc dup2
                    close(pipes[childIndex][WRITE]);  // close write end bc dup2

                    execCmd(childCmd);
                }
                printf("This part only happens if they use exit, cd, or history\n");
                return;
            }
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
}

void
history_add(HistoryLinkedList *history, char *input) {
    // edge case for empty history
    // history of size 1
    // history of

    // if the size == capacity
    //   move the head to the next node, delete the old head,

    // else if the size is 0 make the head and the tail a new node
    //

    // else just make a new node attach it to the current tail

    // to do the last part just
    // make a new tail
    // attach the tail to the current tail


    HistoryListNode* newTail = (HistoryListNode*)malloc(sizeof(HistoryListNode));
    strcpy(newTail->line, input);
    newTail->next = NULL;
    if (history->size == history->capacity) {
        HistoryListNode* oldHead = history->head;
        history->head = history->head->next;
        free(oldHead);
        history->tail->next = newTail;
        history->tail = newTail;
    } else if (history->size == 0) {
        history->head = newTail;
        history->tail = newTail;
        history->size++;
    } else {
        history->tail->next = newTail;
        history->tail = newTail;
        history->size++;
    }
}

void
history_clear(HistoryLinkedList *history) {
    freeHistory(history);
    history_init(history);
}

char *
history_get(HistoryLinkedList *history, int offset) {
    if (offset < 0 || offset >= history->size) {
        return "";
    }
    int index = 0;
    HistoryListNode *current = history->head;
    while (index < offset) {
        current = current->next;
        index++;
    }
    return current->line;
}

void
history_print(HistoryLinkedList *history) {
    HistoryListNode *current = history->head;
    int offset = 0;
    while (current != NULL) {
        HistoryListNode *next = current->next;
        printf("%d:\t %s\n", offset, current->line);
        current = next;
        offset++;
    }
}

void
history_init(HistoryLinkedList *history) {
    history->head = NULL;
    history->tail = NULL;
    history->size = 0;
    history->capacity = 100;
    assert(history->capacity > 0);
}

void
freeHistory(HistoryLinkedList *history) {
    HistoryListNode *current = history->head;
    while (current != NULL) {
        HistoryListNode *next = current->next;
        free(current);
        current = next;
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

bool
isInteger(char* number) {
    for (int i = 0; number[i] != '\0'; i++) {
        if (!isdigit(number[i])) {
            return false;
        }
    }
    return true;
}
