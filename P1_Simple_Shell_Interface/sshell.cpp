#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>

#define MAX_LINE 80 // The maximum length command

int main(void) {
    char *args[MAX_LINE/2 + 1]; // command line arguments
    int should_run = 1; // flag to determine when to exit program

        while (should_run) {
            printf("osh> "); // display prompt
            fflush(stdout);
            
            // read user input
            char input[MAX_LINE + 1]; // +1 for null terminator
            
            /**
             * the following if statements handle:
             * 1. reading input from stdin
             * 2. removing trailing newline character
             * 3. ignoring empty input
             * 4. exiting shell on "exit" command
             */
            if (fgets(input, sizeof(input), stdin) == NULL){
                break; // exit on EOF (ctrl + d) or error
            }
            size_t input_length = strlen(input);

            if (input_length > 0 && input[input_length - 1] == '\n'){
                input[input_length - 1] = '\0'; // remove trailing newline
            }

            if (input[0] == '\0'){
                continue; // ignore empty input
            }

            if (strcmp(input, "exit") == 0){
                should_run = 0; // exit shell if user types "exit"
                continue;
            }

            /*----------------------------------------------------------------------------------*/

            /** 
             * parsing input into arguments
             */    
            char *token = strtok(input, " \t");
            int argc = 0;
            int background = 0; // flag for background execution

            while (token != NULL && argc < MAX_LINE /2){
                args[argc++] = token; // add argument to args array and increment count
                token = strtok(NULL, " \t");
            }

            // check if last argument is '&' for background execution
            if (argc > 0 && strcmp(args[argc - 1], "&") == 0){
                background = 1; // set background flag
                args[--argc] = NULL; // remove '&' from args
            }

            args[argc] = NULL; // null-terminate the args array

            if (argc == 0){
                continue; // no command entered
            }

            /*----------------------------------------------------------------------------------*/
                        
            /**
             * After reading user input, the steps are:
             * (1) fork a child process using fork()
             * (2) the child process will invoke execvp()
             * (3) parent will invoke wait() unless command included &             
             */
            
            pid_t pid = fork(); // (1) create a new process

            if (pid < 0){
                perror("fork"); 
                continue; // start again with next prompt
            } else if (pid == 0){
                // (2) child process
                execvp(args[0], args);
                perror("execvp"); // print error if execvp fails
                _exit(EXIT_FAILURE); // exit child process on failure
            } else {
                // (3) parent process
                if (!background){
                    waitpid(pid, NULL, 0); // wait for child to finish if not background
                } else {
                    printf("Process running in background with PID: %d\n", pid); // print background PID
                }
            }
        }
        return 0;
}