#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <ctype.h>

//Define constants
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

//Define Struct for history list node
struct HistoryListNode {
    char line[BUFFERSIZE];
    struct HistoryListNode *next;
};
typedef struct HistoryListNode HistoryListNode;

//Define history linked list
struct HistoryLinkedList{
    HistoryListNode *head;
    HistoryListNode *tail;
    int capacity;
    int size;
};
typedef struct HistoryLinkedList HistoryLinkedList;

//Create CMD Struct from which commands can be executed
struct CMD {
    char** args;
    bool isPipe;
};
typedef struct CMD CMD;

//Define io functions
char *getInput();
char **getTokens(char *input);
void printTokens(char **tokens);

//Define CMD Building functions
CMD  **getCmds(char **tokens);
void freeCmds(CMD **cmds);
void printCmds(CMD **cmds);
void execCmd(CMD *cmd);
void execCmds(CMD **cmds);

//Define History linked list functions
void history_add(HistoryLinkedList *history, char *input);
void history_clear(HistoryLinkedList *history);
char *history_get(HistoryLinkedList *history, int offset);
void history_init(HistoryLinkedList *history);
void history_print(HistoryLinkedList *history);
void freeHistory(HistoryLinkedList *history);

// utility functions for piping
void exit_err(char *msg);
void make_fork(pid_t *cpid, char *msg);
void make_pipe(int **pipe_fd, char *msg);
void make_dup2(int fd1, int fd2, char *msg);
bool isInteger(char* number);
bool didEarlyExit = false;

HistoryLinkedList history = {0}; // Initialize history with empty values

int main(int argc, char* argv[]) {
    history_init(&history); // Initialize history with null null values and capacity

    while(true) {
        const char* prompt = PROMPT; // Print out sish prompt
        printf("%s", prompt);

        char* input = getInput(); // Get input and add it to history

        history_add(&history, input); // Get input and add the line to history

        char** tokens = getTokens(input); // Parse the input as a series of tokens by the " " delimeter

        CMD** cmds = getCmds(tokens); // Construct commands based on delimited tokens

        execCmds(cmds); // Execute CMDS


        freeCmds(cmds); // Free already used data
        free(tokens);
        free(input);

        if(didEarlyExit){
            break;
        }
    }
    freeHistory(&history); // Free history

    printf("Thank you for using sish!\n");

    return EXIT_SUCCESS;
}

char* getInput() { // Code to read from a line
    size_t bufferSize = BUFFERSIZE;
    char *line = (char*)malloc(sizeof(char) * bufferSize);
    ssize_t bytesRead;
    if ((bytesRead = getline(&line, &bufferSize, stdin)) == -1) {
        printf("This is a non fatal error!\n");
    }
    line[bytesRead - 1] = '\0';
    return line;
}

char** getTokens(char* input) { // Use strtok to grab tokens one by one from the input line
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

void printTokens(char **tokens) { // DEBUGGING METHOD: Print out tokens if wanted
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

        if (strcmp(tokens[tokenIndex], "|") == 0) { // Pipe should end a prior command and create a new "piper" command
            cmd->args[argIndex] = tokens[tokenIndex];
            cmd->isPipe = true;
            argIndex++;
            tokenIndex++;
        } else { // Otherwise, repeatedly read a token until a pipe command or end of line
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
freeCmds(CMD** cmds) { // Free each CMD struct to prevent memory leak
    for (int cmdIndex = 0; cmds[cmdIndex] != NULL; cmdIndex++) {
        free(cmds[cmdIndex]->args);
    }
    free(cmds);
}

void
printCmds(CMD** cmds) { // DEBUGGGING METHOD: Free each CMD struct to prevent memory leak
    for (int cmdIndex = 0; cmds[cmdIndex] != NULL; cmdIndex++) {
        printf("cmd: ");
        for (int argIndex = 0; cmds[cmdIndex]->args[argIndex] != NULL; argIndex++) {
            printf("%s ", cmds[cmdIndex]->args[argIndex]);
        }
        printf("\n");
    }
}

void execCmd(CMD* cmd) { // Execute a system cmd using execvp
    execvp(cmd->args[0], cmd->args);
    char errMsg[BUFFERSIZE];
    sprintf(errMsg, "failed to execvp on \"%s\"", cmd->args[0]);
    exit_err(errMsg);
    return;
}

void execCmds(CMD **cmds) {// Execute a list of cmds
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
    // IDs of each of the children
    pid_t *cpids = (pid_t*)malloc(sizeof(pid_t) * numChildren);

    // Form pipes based on the number of |-type CMD structs passed in
    for (int pipeIndex = 0; pipeIndex < numPipes; pipeIndex++) {
       make_pipe(&pipes[pipeIndex], "failed to make pipe");
    }
    // For each of the CMDs, try to execute
    for (int childIndex = 0; childIndex < numChildren; childIndex++) {
        CMD* childCmd = childrenCmds[childIndex];
        if (strcmp(childCmd->args[0], "exit") == 0) { //Exit if neccessary
            didEarlyExit = true;
            return;
        } else if (strcmp(childCmd->args[0], "cd") == 0) { // Change diretocry
            int dir = chdir(childCmd->args[1]);
            if(dir != 0){
                printf("Failed to change directory to %s\n", childCmd -> args[1]);
            }
        } else if (strcmp(childCmd->args[0], "history") == 0) {
            // check for clear and check for offset
            bool shouldClear = false;
            int offset = -1;
            for (int argIndex = 1; childCmd->args[argIndex] != NULL; argIndex++) {
                if (strcmp(childCmd->args[argIndex], "-c") == 0) { // Clear if needed
                    shouldClear = true;
                } else if (isInteger(childCmd->args[argIndex])) { // Go to offset otherwise if needed
                    offset = atoi(childCmd->args[argIndex]);
                } else {
                    printf("bad arguments supplied to history cmd\n");
                }
            }

            if (shouldClear) {
                //Clear history
                history_clear(&history);
            } else if (offset != -1) {
                //Reexecute old command
                char* line = history_get(&history, offset);
                history_add(&history, line);

                char **tokens = getTokens(line);

                CMD **cmds = getCmds(tokens);
                execCmds(cmds);
            } else{
                //Print history
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

void history_add(HistoryLinkedList *history, char *input) {
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

void history_clear(HistoryLinkedList *history) {
    // Free and reinitialize history to clear
    freeHistory(history);
    history_init(history);
}

char* history_get(HistoryLinkedList *history, int offset) {
    //Go to offset and return the line at that node
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

void history_print(HistoryLinkedList *history) {
    // Print history through standard iteration of a linked list
    HistoryListNode *current = history->head;
    int offset = 0;
    while (current != NULL) {
        HistoryListNode *next = current->next;
        printf("%d:\t %s\n", offset, current->line);
        current = next;
        offset++;
    }
}

void history_init(HistoryLinkedList *history) {
    // Initialize history with correct default values
    history->head = NULL;
    history->tail = NULL;
    history->size = 0;
    history->capacity = 100;
    assert(history->capacity > 0);
}

void freeHistory(HistoryLinkedList *history) {
    // Remove all linked list nodes
    HistoryListNode *current = history->head;
    while (current != NULL) {
        HistoryListNode *next = current->next;
        free(current);
        current = next;
    }
}

void exit_err(char* msg) {
    // If neccessary, exit sish
    perror(msg);
    exit(EXIT_FAILURE);
}

void make_fork(pid_t* cpid, char* msg) {
    //Fork from a cpid and throw error otherwise
    if ((*cpid = fork()) < 0) {
        exit_err(msg);
    }
}

void make_pipe(int** pipe_fd, char* msg) {
    //Fork from a pipe and throw error otherwise
    *pipe_fd = (int*)malloc(2 * sizeof(int));
    if (pipe(*pipe_fd) == - 1) {
        exit_err(msg);
    }
}

void make_dup2(int fd1, int fd2, char* msg) {
    //Duplicate file descriptor to a new file descriptor as neccessary as neccessary
    if ((dup2(fd1, fd2)) == -1) {
        exit_err(msg);
    }
}

bool isInteger(char* number) {
    //Check if a number is an integer to create offsets
    for (int i = 0; number[i] != '\0'; i++) {
        if (!isdigit(number[i])) {
            return false;
        }
    }
    return true;
}
