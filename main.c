#include <sys/wait.h> // for waitpid
#include <stdio.h>    // for printf and perror
#include <stdlib.h>   // for exit
#include <unistd.h>   // for execv, getpid, fork
#include <string.h>  // For str
#include <signal.h> // For signals
#include <sys/types.h> // process ids
#include <fcntl.h> // file control options


int maxCharacters = 2048; // max # characters in command
int maxArgs = 512; // max # arguments in command
int childExitMethod;
int backgroundAllowed = 1; // For foreground only mode

/* 
Handles SIGSTP. Sets background processes to allowed/not allowed depending on when called.
If not allowed & will be ignored and all processes will be run as foreground
*/
void handle_SIGTSTP(int sig)
{
    // if backgroundAllowed == 1, changes to 0 to prevent & disable background task
    if (backgroundAllowed == 1)
    {
    char* message = "Entering foreground-only mode (& is now ignored)\n";
    write (STDOUT_FILENO, message, 49);
    fflush(stdout);
    backgroundAllowed = 0;
    } else // if backgroundAllowed == 0, changes to 0 to prevent & enabling background task
    {
    char* message = "Exiting foreground-only mode\n";
    write (STDOUT_FILENO, message, 28);
    fflush(stdout);
    backgroundAllowed = 1;
    }
}


/* 
Prints exit status or termination desired process id
*/
int exitStatus(int exitMethod, int* lastForegroundPid)
{
    // Checks if terminated by status
    if (WIFEXITED(exitMethod))
    {
        int exitStatus = WEXITSTATUS(exitMethod);
        printf("Process id %d has terminated: termination status was %d\n", *lastForegroundPid, exitStatus);
        fflush(stdout);
    } 
    // Checks if terminated by signal
    else if (WIFSIGNALED(exitMethod))
    {
        int termSignal = WTERMSIG(exitMethod);
        printf("Process id %d has terminated: terminated by signal %d\n", *lastForegroundPid, termSignal);
        fflush(stdout);
    }
}

/* 
Takes user input in form of argument. If first argument passed by user is exit, cd or status
appropriate function is run. If not, the command is run as child process by shell
*/
void runShell(char *newargv[], char inputFile[], char outputFile[], int pidArray[], int background, int* lastBackgroundPid, int* lastForegroundPid, struct sigaction SIGINT_action, struct sigaction SIGTSTP_action)
{
    // Check if recieving empty argument array, catch and return if so
    if (newargv[0] == NULL)
    {
        return;
    }
    
    char *dir = getenv("HOME"); // Home directory 
    int arg = 0; // # of arguemnts passed to shell
    pid_t pid = getpid(); // process id of parent
    int i; // used to iterate

    // cd command
    if (strcmp("cd", newargv[0]) == 0)
    {
        // if directory not specified, change to HOME
        if(newargv[1] == NULL)
        {
            chdir(dir);
        }
        // change to specified directory
        else
        {
            chdir(newargv[1]);
        }
    }

    // status command
    else if (strcmp("status", newargv[0]) == 0)
    {
        // Check if status run before foreground status
        if(*lastForegroundPid == 0)
        {
            printf("Exit Status: 0\n");
            fflush(stdout); 
        } else // return exit status
        {
        exitStatus(childExitMethod, lastForegroundPid);
        }
    }

    // exit command
     else if (strcmp("exit", newargv[0]) == 0)
    {
        // Check user is entering exit command
        if(newargv[1] == NULL)
        {
            int x;
            printf("Terminatting running processes before exiting...\n");
            fflush(stdout);
            // Loop trough process ID array
            for (x = 0; x < 101; x++)
            {
                // Terminate signal if greater than 0
                if (pidArray[x] != 0)
                {
                printf("Process %d terminated\n", pidArray[x]);
                fflush(stdout);
                kill(pidArray[x], SIGKILL);
                }
            }
            // Terminate parent
            printf("Terminating Self\n");
            free(lastBackgroundPid);
            free(lastForegroundPid);
            kill(pid, SIGKILL);
            fflush(stdout);
            exit(0);
        }
    }
    
    // Executing other commands
    else 
    {
        // initialize to bogus value to get return value
        pid_t spawnpid = -5;
        int backgroundchildmethod;

        // spawnpid contains return value of fork. 0 if child is running, -1 error, any other value we are in a parent
        fflush(stdout);
        spawnpid = fork();

        switch(spawnpid)
        {
            // no child process was created. Set exit status to 1.
            case -1:
                perror("Hull Breach");
                exit(1);
                break;

            // inside child process
            case 0:

            // Set child process to terminate from sigint signal in foreground process
            if (background==0)
            {
            SIGINT_action.sa_handler = SIG_DFL;
            sigaction(SIGINT, &SIGINT_action, NULL);
            }

            // redirecting file for output
            if (strlen(outputFile) != 0)
            {
                // Create file, if exist truncate
                int oFile = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                // output file could not open
                if (oFile == -1)
                {
                    printf("Error could not open file\n");
                    fflush(stdout);
                    exit(1);
                }
                // duplicate file into stdout. Sets stdout to file for printing
                int oFile2 = dup2(oFile, STDOUT_FILENO);
                // output file could not open
                if (oFile2 == -1)
                {
                    printf("Error could not open file\n");
                    fflush(stdout);
                    exit(1);
                }
                // close file
                close(oFile);
            }

            // redirecting file for input
            if (strlen(inputFile) != 0)
            {
                // Open file for reading
                int iFile = open(inputFile, O_RDONLY);
                // file could not open
                if (iFile == -1)
                {
                    printf("cannot open badfile for input\n");
                    fflush(stdout);
                    exit(1);
                }
                // duplicate file into stdin. Sets stdin to file for printing
                int iFile2 = dup2(iFile, STDIN_FILENO);
                // file could not open
                if (iFile2 == -1)
                {
                    printf("cannot open badfile for output\n");
                    fflush(stdout);
                    exit(1);
                }
                // close file
                close(iFile);
            }

            // replace current process using command from user
            fflush(stdout);
            if (execvp(newargv[0], newargv) < 0)
            {                  
            perror("Exec failure!");
            exit(1);
            }
            break;

            // in parent process
            default:
                // forground process
                if (background == 0)
                {
                    // Store last foreground process id, used by status command
                    *lastForegroundPid = waitpid(spawnpid, &childExitMethod, 0);
                }
                else
                // background process
                {
                // check if any process has returned, assign to lastBackgroundPid
                    *lastBackgroundPid = waitpid(spawnpid, &backgroundchildmethod, WNOHANG);
                    printf("background pid is %d\n", spawnpid);
                    fflush(stdout);

                    // Append current process if to pid array
                    for(i=0; i < sizeof(pidArray); i++) {
                        if(pidArray[i] == 0) 
                        {
                        pidArray[i] = spawnpid;
                        break;
                        }
                    }
                    break;
                }
            
        }
    }
    // Remove completed background processes from process array
    int backgroundExit = -5;
    while(1){
        
        // gets process id of last completed process
        *lastBackgroundPid = waitpid(-1, &backgroundExit, WNOHANG);
        
        // if non zero, remove from process ids
        if (*lastBackgroundPid > 0)
        {
        for(i=0; i < sizeof(pidArray); i++)
            if(pidArray[i] == *lastBackgroundPid)
                {
                pidArray[i] = 0;
                exitStatus(backgroundExit, lastBackgroundPid);
                break;
                }

        } else // process id is 0, break from loop
        {
            break;
        }
    }
}

/* 
Gets the command from the user
*/
int getCommand(char *newargv[], char inputFile[], char outputFile[])
{
    // Gets process id to convert $$ to smallsh's process id
    pid_t pid = getpid();
    char stringPid[10];

    // Convert process id to character array
    sprintf(stringPid, "%d", pid);

    // array for user input
    char userInput[maxCharacters];

    // # of arguemnts passed to shell
    int arg = 0;
    char * token;
    int i;

    // clear arrays
    memset(newargv, 0, maxArgs);
    memset(inputFile, 0, 256);
    memset(outputFile, 0, 256);

    // Flush output buffers
    fflush(stdout);
    fflush(stdin);

    // Get input from user
    printf(": ");
    fgets(userInput, maxCharacters, stdin);

    // Check if user entered blank line or comment
    if (userInput[0] == '\n' || userInput[0] == '#')
    {
        return 0;
    }

    // Remove /n from end of userInputs
    userInput[strlen(userInput) - 1] = 0;

    // Break userInput into arguments
    token = strtok(userInput, " ");

    // Loop while argument is present
    while (token != NULL)
    {
        int i =0;
        int variableCount=0;
        // copy of token, to be modified
        char *s2 = strdup(token);
       
        // Check if user adding file for input 
        if (strcmp(token, "<") == 0)
        {
            // Move token passed <, name of input file and copy to array
            token = strtok(NULL, " ");
            if (token != NULL)
            {
                strcpy(inputFile, token);
            } else // no filename passed, /dev/null used instead
            {
                strcpy(inputFile, "/dev/null");
            }
            
        }

        // Check if user adding file for output 
        if (strcmp(token, ">") == 0)
        {
            // Move token passed <, name of output file and copy to array
            token = strtok(NULL, " ");
            if (token != NULL)
            {
                strcpy(outputFile, token);
            }
            else // no filename passed, /dev/null used instead
            {
                strcpy(outputFile, "/dev/null");
            }
        }

        // Loop over copy of token, checking to see if it contains $$ and replacing it with process id if so
        while (s2[i])
        {
            // $ character in token, increment variable counter
            if(s2[i] == '$')
            {
                variableCount++;
            }
            // Other character deteceted, reset variable counter 
            else
            {
                variableCount=0;
            }
            // $$ is in string, replaced with process if of smallsh
            if(variableCount == 2)
            {
                char* substring = strstr(s2, "$$");
                // // move substring
                memmove(substring + strlen(stringPid), substring + strlen("$$"), strlen(substring) - strlen("$$") + 1);
                // // replace substring with process id
                memcpy(substring, stringPid, strlen(stringPid));
                // // increment i to current new place in string
                i = i + (strlen(stringPid) - 1);
                // variableCount=0;
            }
            else
            {
                i++;
            }
        }
        // Add argument to array and move token to next string in input
        if(*s2 != '<' && *s2 != '>' && *s2 !='&')
        {
        newargv[arg] = s2;
        }
        token = strtok(NULL, " ");
        arg += 1;

        // Check if process to be run in background
        if(token == '\0' && *s2 == '&')
        {
            // Check for foreground only mode
            if (backgroundAllowed == 1)
            {
                newargv[arg] = NULL;
                return 1;
            } else { // No background process allowed
                newargv[arg] = NULL;
                return 0;
            }
            
        }
    }
    
    // Set last argument to NULL
    newargv[arg] = NULL;
    return 0;
}

int main(){

    // array for arguments
    char *newargv[maxArgs];
    // array for name of inputFile
    char inputFile[256];
    // array for name of output file
    char outputFile[256];
    // array of process ids
    int pidArray[100];
    // last removed background process id
    int *lastBackgroundPid;
    lastBackgroundPid = malloc(sizeof(int));
    *lastBackgroundPid = 0;
    // last removed foreground process id
    int *lastForegroundPid;
    lastForegroundPid = malloc(sizeof(int));
    *lastForegroundPid = 0;
    // Last pid terminated by sigint. Used to prevent message from firing more than once
    int lastSigIntDisplayed;


    // Struct for handling SIGINT
    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = SIG_IGN; // set to handle sigint
    sigfillset(&SIGINT_action.sa_mask); // No signal will interup this signal
    SIGINT_action.sa_flags = 0; // no flags
    sigaction(SIGINT, &SIGINT_action, NULL);


    // Struct for handling SIGSTP
    struct sigaction SIGTSTP_action = {0};
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = 0;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);


    // get user input and runs shell until user exits program using exit command
    while (1){

    // Flush output buffers
    fflush(stdout);
    fflush(stdin);

    // if child process terminated by sigint, return process id and termination signal to user 
    if (WIFSIGNALED(childExitMethod) && lastSigIntDisplayed != *lastForegroundPid)
    {
        int termSignal = WTERMSIG(childExitMethod);
        if (termSignal == 2)
        {
        printf("\nProcess id %d has terminated by SIGINT. Terminated by signal %d\n", *lastForegroundPid, termSignal);
        fflush(stdout);
        lastSigIntDisplayed = *lastForegroundPid;
        }
    }
    

    // Gets input from user, converts input into arguments. Sets background to 1 if background process.
    int background = getCommand(newargv, inputFile, outputFile);

    // Runs shell based on user input
    runShell(newargv, inputFile, outputFile, pidArray, background, lastBackgroundPid, lastForegroundPid, SIGINT_action, SIGTSTP_action);
    }
    free(lastBackgroundPid);
    free(lastForegroundPid);
    return 0;
}