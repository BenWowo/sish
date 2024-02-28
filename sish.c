#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

// start Repl.h
// end Repl.h
// start Repl.c 
// end Repl.c

int main() {
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
        // I can optionally add an EOF to the end of the token stream
        // tokens[numTokens] = EOF;
        // numTokens++;

        printf("numTokens: %d\n", numTokens);
        for (int i = 0; i < numTokens; i++){
            printf("Token: %s\n", tokens[i]);
        }

        // token types
        // bin, flag, ident?


        // can have a cmd struct
        // which will store the cmd and the flags
        // then can convert tokens into list of cmds
        // with this we then know how many forks we will do
        // or could decide to do the forks on the fly

        // for (int i = 0; i < numTokens; i++){
        //     // error validation
            
        //     // read the cmd
        //     // only need to create pipe if using pipe
        //     // I guess I could create a pipe in every case and pipe the output to stdout
        //     int pipe_fd[2];
        //     int child_pid; 

        //     if(pipe(pipe_fd) == -1) {
        //         perror("pipe");
        //         exit(EXIT_FAILURE);
        //     }
        //     if ((child_pid = fork()) == -1) {
        //         perror("fork");
        //         exit(EXIT_FAILURE);
        //     }

        //     if (child_pid == 0) {
        //         close(pipe_fd[0]) // close read end of pipe

        //         // args are just cmd [flags] [thing]
        //         char* args = // get number of args in cmd
        //         for (int i = 0; i < numArgs; i++){
        //             args[i] = // cmd.args[i]
        //         }

        //         // exec then send output to pipe
        //     }
        // }



        // also need to close stdout when printing output of a file
        // also if piping then you need to create the pipe before creating the child process
        // execvp - number of args it numTokens + 1

        // char* args[2];
        // // need to create a child process for teach of the different programs
        // int rc = fork();
        // if (rc == 0) {
        //     for (int i = 1; i < numTokens; i++) {
        //         args[i] = tokens[i];
        //     }    

        //     for (int i = 1; i < numTokens; i++) {
        //         printf("arg: %s\n", tokens[i]);
        //     }
        //     printf("Does it get here?\n");

        //     return 0;
        // }

    }
    printf("Thank you for using sish!\n");
}