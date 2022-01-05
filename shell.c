#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <math.h>
#include <signal.h>

/*
 * Shell implementation project
 * This program will be an implementation of a shell
 * Basic commands like ls, wc, pwd, and other shell commands are done using execvp
 * It will be able to perform exit, cd and history
 * It will also be able to handle pipes for the basic shell commands but not for the built-in commands
 */

#define MAXHISTORY 1000 // total commands that can be stored in history

// struct to hold info about the input
struct inputStuff
{
    char *line; // Stores input string
    size_t len;
    ssize_t read; // Length of the string
    char *token;
    char *dlims;      // Delimiters list
    char *command;    // stores command
    char **args;      // stores arguments of command
    int num_tokens;   // number of tokens from input
    char **pipedlist; // list of piped commands
    int num_pipes;
    int invalidIndex; // if history command was invalid index, you can't run it if called by another history
};

static struct inputStuff histlist[MAXHISTORY]; // Array to store command history (including failed ones)

// function prototypes
void addHistory(unsigned int *counthist, struct inputStuff *inputLine); // adds line to history list
void tokenize(struct inputStuff *inputLine); // breaks line into command and its arguments
struct inputStuff getInput(unsigned int *counthist); // gets user input
void cd(struct inputStuff *input); // changes the current working directory
void runHistory(struct inputStuff *input, unsigned int *counthist); // executes the history command based on the argument
int execute(struct inputStuff *input); // executes the command if not built-in
void checkCommand(struct inputStuff *input, unsigned int *counthist); // decides what function needed to execute the user input
void runPipedCommands(struct inputStuff *input, unsigned int *counthist, struct inputStuff *comms); // runs a piped command (only if it isn't built in commands)
void tokenizePipe(struct inputStuff *inputLine); // break up line into piped commands
int notbuiltin(struct inputStuff *input); // checks if command is exit, cd or history
void runCommand(struct inputStuff *input, unsigned int *counthist); // used to decide how to run the program (pipe command or built-in)

void err_exit(char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

// returns 0 if built in command, 1 otherwise
int notbuiltin(struct inputStuff *input)
{
    char done[] = "exit"; // string to see if command was exit
    int cmpdone;
    char change[] = "cd"; // string to see if command was change directory
    int cmpchange;
    char history[] = "history"; // string to see if command was history
    int cmphist;

    if (input->command == NULL){
        return -1;
    }
    // Check if command was exit, cd, or history and execute
    cmpdone = strcmp(input->command, done);
    cmpchange = strcmp(input->command, change);
    cmphist = strcmp(input->command, history);

    if (cmpdone == 0 || cmpchange == 0 || cmphist == 0)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}
// function to perform piping
void runPipedCommands(struct inputStuff *input, unsigned int *counthist, struct inputStuff *comms)
{

    // create pipes for number of pipes
    // 2 pipefd per pipe
    int pipefd[input->num_pipes][2];

    pid_t pid = 0;

    // open enough pipes
    int j;
    for (j = 0; j < input->num_pipes; j++)
    {
        if (pipe(pipefd[j]) < 0)
        {
            err_exit("pipe");
        }
    }

    // fork pipes together and execute command
    int k;
    for (k = 0; k < input->num_pipes; k++)
    {
        pid = fork();
        if (pid == 0)
        {
            // read from pipe
            if (k != 0)
            {
                close(pipefd[k - 1][1]); // close write end
                if (dup2(pipefd[k - 1][0], STDIN_FILENO) != STDIN_FILENO)
                {
                    err_exit("stdin");
                }
                close(pipefd[k - 1][0]); // close read end
            }

            // write to pipe
            close(pipefd[k][0]);
            if (dup2(pipefd[k][1], STDOUT_FILENO) != STDOUT_FILENO)
            {
                err_exit("stdout");
            }
            close(pipefd[k][1]);

            // execute command
            execvp(comms[k].command, comms[k].args);
            err_exit("invalid command");
        }
        else
        {
            if (k != 0)
            {
                // close other pipes
                close(pipefd[k - 1][0]);
                close(pipefd[k - 1][1]);
            }
        }
    }

    // last command
    if (k > 0)
    {
        close(pipefd[k - 1][1]);
        if (dup2(pipefd[k - 1][0], STDIN_FILENO) != STDIN_FILENO)
        {
            err_exit("stdout1");
        }
        close(pipefd[k - 1][0]);
    }
    execvp(comms[k].command, comms[k].args);
    err_exit("invalid command");
}

// function to add to history
void addHistory(unsigned int *counthist, struct inputStuff *inputLine)
{
    // Add line to history
    if (*counthist < MAXHISTORY)
    {
        histlist[*counthist].line = strdup(inputLine->line);
        *counthist = *counthist + 1;
    }

    // If there are already max commands in history, delete the first one and shift everything back
    else
    {
        int j;
        for (j = 1; j < MAXHISTORY; j++)
        {
            histlist[j - 1] = histlist[j];
        }
        histlist[MAXHISTORY - 1].line = strdup(inputLine->line);
    }
}

// function to tokenize string
void tokenize(struct inputStuff *inputLine)
{
    // tokenizing line
    // The input tokens are delimited by " "

    inputLine->token = strtok(inputLine->line, inputLine->dlims);

    // First token is command
    inputLine->command = inputLine->token;

    // Index for number of arguments
    int i = 0;

    // Array of arguments initialized to 1 argument (1st arg is command)
    inputLine->args = (char **)malloc(sizeof(char *) * 1);
    if (inputLine->args == NULL)
    {
        perror("error allocating memory");
    }
    inputLine->args[0] = " ";
    i++;

    // Break up arguments and put into array
    while (inputLine->token != NULL)
    {
        inputLine->num_tokens = inputLine->num_tokens + 1;
        inputLine->token = strtok(NULL, inputLine->dlims);

        if (inputLine->token == NULL)
        {
            break;
        }
        else
        {
            char **temp = realloc(inputLine->args, ++inputLine->num_tokens * sizeof(char *));
            if (temp == NULL)
            {
                perror("error reallocating memory");
                free(temp);
            }
            else
            {
                inputLine->args = temp;
                inputLine->args[i] = strdup(inputLine->token);
                i++;
            }
        }
    }
    inputLine->args[i] = NULL; // NULL terminate args array
}

// function to tokenize line into piped commands
void tokenizePipe(struct inputStuff *inputLine)
{
    // tokenizing line
    // The input tokens are delimited by "|"

    inputLine->token = strtok(inputLine->line, inputLine->dlims);

    // Index for number of arguments
    int i = 0;

    // Array of arguments initialized to 1 argument (1st arg is command)
    inputLine->pipedlist = (char **)malloc(sizeof(char *) * 1);
    if (inputLine->pipedlist == NULL)
    {
        perror("error allocating memory");
    }
    // add first token to pipedlist
    inputLine->pipedlist[0] = inputLine->token;
    i++;

    inputLine->num_tokens = inputLine->num_tokens + 1;

    // Break up arguments and put into array
    while (inputLine->token != NULL)
    {

        inputLine->token = strtok(NULL, inputLine->dlims);

        if (inputLine->token == NULL)
        {
            break;
        }
        else
        {
            char **temp = realloc(inputLine->pipedlist, ++inputLine->num_tokens * sizeof(char *));
            if (temp == NULL)
            {
                perror("error reallocating memory");
                free(temp);
            }
            else
            {
                inputLine->pipedlist = temp;
                inputLine->pipedlist[i] = strdup(inputLine->token);
                i++;
            }
        }
    }
    inputLine->num_pipes = inputLine->num_tokens - 1;
}

// function to get input line
struct inputStuff getInput(unsigned int *counthist)
{
    struct inputStuff inputLine;

    // default struct values
    inputLine.len = 0;
    inputLine.dlims = " ";
    inputLine.num_tokens = 0;
    inputLine.token = NULL;

    // reading line from user input
    inputLine.read = getline(&inputLine.line, &inputLine.len, stdin);
    while ((inputLine.read == -1 || (strlen(inputLine.line) == 1)))
    {
        printf("Please enter a valid string: ");
        free(inputLine.line);
        inputLine.line = NULL;
        inputLine.read = getline(&inputLine.line, &inputLine.len, stdin);
    }

    if (*inputLine.line && inputLine.line[strlen(inputLine.line) - 1] == '\n')
    {
        inputLine.line[strlen(inputLine.line) - 1] = '\0';
    }

    // add command to history
    addHistory(counthist, &inputLine);

    inputLine.dlims = "|"; // change dlims
    tokenizePipe(&inputLine); //tokenize based on pipes

    return inputLine;
}

// function to do cd command
void cd(struct inputStuff *input)
{
    // cd to home if no args given
    if (input->args[1] == NULL)
    {
        printf("no directory specified, cd to HOME directory...\n");
        if (chdir(getenv("HOME")) != 0)
        {
            printf("failed to cd to root\n");
        }
    }
    // cd to specified directory
    else if (chdir(input->args[1]) != 0)
    {
        printf("invalid directory\n");
    }
}

// function to do history command
void runHistory(struct inputStuff *input, unsigned int *counthist)
{
    char cflag[] = "-c"; // string to see if clear flag was chosen
    int flagcmp;

    // if -c then clear all history
    if (input->args[1] != NULL)
    {
        flagcmp = strcmp(input->args[1], cflag);
        if (flagcmp == 0)
        {
            // make all elements null
            int i;

            // null struct
            struct inputStuff empty;
            empty.line = NULL;
            empty.invalidIndex = 0; // not invalid anymore
            for (i = 0; i < *counthist; i++)
            {
                histlist[i] = empty;
            }

            // reset history count to 0
            *counthist = 0;
        }
        else
        {

            // check if offset passed in
            if (!isdigit(input->args[1][0]))
            {
                printf("invalid flag, only \"-c\" is supported\n");
            }
            // execute command that is chosen
            else
            {

                struct inputStuff n;
                // // rerun program with new command
                if (atoi(input->args[1]) < *counthist-1 && (histlist[atoi(input->args[1])].invalidIndex != 1))
                {
                    // set struct values
                    char *newLine = NULL;
                    newLine = strdup(histlist[atoi(input->args[1])].line);
                    n.line = strdup(newLine);
                    n.len = 0;
                    n.num_tokens = 0;
                    n.token = NULL;
                    n.dlims = "|";

                    tokenizePipe(&n); // tokenize based on pipes

                    // execute command
                    runCommand(&n, counthist);

                }
                
                // invalid index means it won't run
                else
                {
                    histlist[atoi(input->args[1])].invalidIndex = 1;
                    printf("invalid command\n");
                }
            }
        }
    }
    // otherwise show history
    else
    {
        int k;
        for (k = 0; k < *counthist; k++)
        {
            printf("%d %s\n", k, histlist[k].line);
        }
    }
}

// function to execute commands
int execute(struct inputStuff *input)
{
    int done = 0;

    // execvp using forked process
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork failed");
        done = 1;
    }
    // child does this
    else if (pid == 0)
    {

        if (execvp(input->command, input->args) < 0)
        {
            // error if invalid
            printf("invalid command\n");
            done = 1;
        }
    }
    // parent does this
    else
    {
        // wait for child to finish
        wait(NULL);
    }

    return done;
}

// function to see which command to execute
void checkCommand(struct inputStuff *input, unsigned int *counthist)
{
    char done[] = "exit"; // string to see if command was exit
    int cmpdone;
    char change[] = "cd"; // string to see if command was change directory
    int cmpchange;
    char history[] = "history"; // string to see if command was history
    int cmphist;

    // Check if command was exit, cd, or history and execute
    cmpdone = strcmp(input->command, done);
    cmpchange = strcmp(input->command, change);
    cmphist = strcmp(input->command, history);

    // exit command used
    if (cmpdone == 0)
    {
        _exit(EXIT_SUCCESS);
    }

    // cd command used
    if (cmpchange == 0)
    {
        cd(input);
    }

    // history command used
    else if (cmphist == 0)
    {
        runHistory(input, counthist);
    }

    // fork program to execute commands
    else
    {
        int done = 0;
        done = execute(input);
        if (done == 1)
        {
            exit(EXIT_FAILURE);
        }
    }
}

void runCommand(struct inputStuff *input, unsigned int *counthist)
{
    // create struct for every command
    struct inputStuff comms[input->num_pipes + 1]; // initialize struct array
    int i;
    for (i = 0; i <= input->num_pipes; i++)
    {
        char *tempLine = NULL;
        tempLine = input->pipedlist[i];
        comms[i].line = tempLine;
        comms[i].len = 0;
        comms[i].dlims = " ";
        comms[i].num_tokens = 0;
        comms[i].token = NULL;
        tokenize(&comms[i]);
    }

    // check if builtin command and at least 1 pipe
    if (input->num_pipes > 0)
    {
        for (i = 0; i <= input->num_pipes; i++)
        {
            int built = notbuiltin(&comms[i]);
            // if command is builtin, don't do piping
            if (built == 0)
            {
                printf("built-in commands not supported with piping\n");
                return;
            }
            if (built == -1){
                printf("invalid syntax\n");
                return;
            }
        }
    }

    // if no pipes and built in command
    if (input->num_pipes == 0 && notbuiltin(&comms[0]) == 0)
    {
        checkCommand(&comms[0], counthist);
    }

    // do piped command if none are built in by forking new process
    else
    {
        pid_t pipefork;
        pipefork = fork();
        if (pipefork < 0)
        {
            err_exit("fork");
        }
        if (pipefork == 0)
        {

            runPipedCommands(input, counthist, comms);
        }
        wait(NULL);
    }
}
int main(int argc, char *argv[])
{
    unsigned int counthist = 0; // variable to store number of items in history

    // loop to run program
    while (1)
    {
        printf("sish> ");
        // get user input and do the command
        struct inputStuff input = getInput(&counthist);
        runCommand(&input, &counthist);
    }

    exit(EXIT_SUCCESS);
}
