#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>


// utilitie
#define READ 0
#define WRITE 1

// stuff
struct CMD {
    char* command;
    char* flag;
}; typedef struct CMD CMD;

void exit_err(char* msg, int code);
void make_fork(int* cpid, char* msg, int errCode);
void make_pipe(int** pipe_fd, char* msg, int errCode);
void make_dup2(int fd1, int fd2, char* msg, int errCode);

CMD* build_cmds(char** tokens, int numTokens, int* numCmds);


int main(int argc, char* argv[]) {
    const char* prompt = "sish> ";
    const size_t MAX_INPUT_LENGTH = 1028;
    char input[MAX_INPUT_LENGTH];

    const size_t MAX_TOKEN_LENGTH = 1028;
    const size_t MAX_NUM_TOKENS = 1028;
    while(1) {
        // Read Eval Print Loop

        // Read
        printf("%s", prompt);
        fgets(input, MAX_INPUT_LENGTH, stdin);
        input[strlen(input) - 1] = '\0';
        // scanf("%s", input);

        // Eval
        int numTokens = 0;
        char *tokens[MAX_NUM_TOKENS];
        char *token, *position;
        char *delimiters = " ";

        token = strtok_r(input, delimiters, &position);
        tokens[numTokens] = token;
        numTokens++;
        while((token = strtok_r(NULL, delimiters, &position)) != NULL) {
            tokens[numTokens] = token;
            numTokens++;
            // have to distinguish between tokens that will be different processes vs flags of process
        }
        // add EOF token to allow for token peaking without going out of bounds
        tokens[numTokens] = "\n";

        printf("numTokens: %d\n", numTokens);
        for (int i = 0; i < numTokens; i++){
            printf("Token: %s\n", tokens[i]);
        }

        int numCmds;
        CMD* cmds = build_cmds(tokens, numTokens, &numCmds); // TODO: Create build_cmds which will turn the list of tokens into a list of CMDs from the | symbol

        // token types
        // bin, flag, ident?

        // can have a cmd struct
        // which will store the cmd and the flags
        // then can convert tokens into list of cmds
        // with this we then know how many forks we will do
        // or could decide to do the forks on the fly

        // also need to close stdout when printing output of a file
        // also if piping then you need to create the pipe before creating the child process
        // execvp - number of args it numTokens + 1

        // ### This is proof of concept for the piping ###
        // All of the command outputs will be written to a buffer
        // for each command a child process is spawned that will
        // 1. Create a pipe
        // 2. Write the contents of the buffer to the pipe
        // 3. Spawn a grandchild that
        //    3.1. reads from the pipe
        //    3.2. executes cmd with inputs from pipe
        //    3.3. writes its output to the pipe
        // 4. The child then reads the contents of the pipe and writes them to the buffer
        //    so that the outputs can be used in piped commands or printed from the main process

        const int BUFFERSIZE = 2048;
        char buffer[BUFFERSIZE];

        printf("\n");
        for (int i = 0; i < numCmds; i++) {
            printf("%s %s\n", cmds[i].command, cmds[i].flag);
            pid_t cpid1;
            make_fork(&cpid1, "failed to fork for cpid1", 1);
            if (cpid1 == 0) {
                int* fd;
                make_pipe(&fd ,"failed to create pipe", 1);

                // write buffer contents to pipe
                size_t contentSize = strlen(buffer) + 1;
                if (write(fd[WRITE], buffer, contentSize) != contentSize) {
                    perror("failed to write buffer content to pipe");
                    return 1;
                }

                pid_t cpid2;
                make_fork(&cpid2, "failed to fork for cpid2", 1);
                if (cpid2 == 0) {
                    close(fd[READ]);
                    make_dup2(fd[WRITE], STDIN_FILENO, "dup2 failed in class child process", 1);
                    close(fd[WRITE]);
                    //TODO, construct commands based on current cmds[i]
                    char* myargs[] = {cmds[i].command, cmds[i].flag, NULL};
                    //TODO, implement the command and run using execvp
                    execvp(myargs[0], myargs);
                }
                close(fd[WRITE]);
                waitpid(cpid2, NULL, 0);

                // write the output of the pipe to the buffer
                int nbytes = read(fd[READ], buffer, sizeof(buffer));
                close(fd[READ]);
                free(fd);
            }
        }
        // print the contents of the buffer to stdout
        printf("This should be the buffer content\n");
        printf("%s\n", buffer);
    }
    printf("Thank you for using sish!\n");
}

void exit_err(char* msg, int code) {
    perror(msg);
    exit(code);
}

void make_fork(int* cpid, char* msg, int code) {
    if ((*cpid = fork()) < 0) {
        exit_err(msg, code);
    }
}

void make_pipe(int** pipe_fd, char* msg, int code) {
    *pipe_fd = (int*)malloc(2 * sizeof(int));
    if (pipe(*pipe_fd) == - 1) {
        exit_err(msg, code);
    }
}

void make_dup2(int fd1, int fd2, char* msg, int code) {
    if ((dup2(fd1, fd2)) == -1) {
        exit_err(msg, code);
    }
}

CMD* build_cmds(char** tokens, int numTokens, int* numCmds){
    CMD* cmds = (CMD*) malloc(sizeof(CMD)*numTokens);

    bool canStartNextCmd = true;
    *numCmds = 0;

    for(int i = 0; i < numTokens; i++){
        int endOfCommand = strcmp(tokens[i], "|") == 0;
        if(endOfCommand){
            //End the current command and start editing the next one
            canStartNextCmd = true;
            (*numCmds)++;
            continue;
        }
        else if(canStartNextCmd){
            //Start building a new command
            cmds[*numCmds].command = tokens[i];
            canStartNextCmd = false;
            continue;
        }else{
            //Build the flags
            cmds[*numCmds].flag = tokens[i];
            continue;
        }
    }
    (*numCmds)++;
    return cmds;
}
