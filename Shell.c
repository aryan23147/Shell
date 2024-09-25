#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>

#define MAX_SIZE_CMD 256
#define MAX_SIZE_ARG 16
#define MAX_PIPES 5
#define MAX_HISTORY 50

char cmd[MAX_SIZE_CMD]; // holds the command
char *argv[MAX_SIZE_ARG];   // an array for arguments and command
pid_t pid;  // global variable for the child process ID
char i;

typedef struct
{
    char command[MAX_SIZE_CMD]; // command string
    pid_t pid;  // process ID of the command
    double exec_time;    // time to execute the command in seconds
    struct timeval start_time;  // time when the command started
} CommandHistory;

CommandHistory history[MAX_HISTORY];    // history array with additional details
int history_count = 0;  // counter for history

void getCmd()
{
    // get command string from user
    printf("Welcome, my master. I am Shell>>    ");

    if (fgets(cmd, MAX_SIZE_CMD, stdin) == NULL)
    {
        perror("Error reading command");
        exit(EXIT_FAILURE);
    }
    if ((strlen(cmd) > 0) && (cmd[strlen(cmd) - 1] == '\n'))
    {
        cmd[strlen(cmd) - 1] = '\0';
    }
}

void printFullHistory()
{
    // print full command history on exit
    printf("Full command history:\n");
    for (int i = 0; i < history_count; i++)
    {
        // convert start time to human-readable format
        struct tm* start_time_info;
        char start_time_str[30];
        start_time_info = localtime(&history[i].start_time.tv_sec);

        if (start_time_info == NULL)
        {
            perror("Error converting start time");
            continue;
        }
        strftime(start_time_str, sizeof(start_time_str), "%Y-%m-%d %H:%M:%S", start_time_info);

        printf("Command: %s\n", history[i].command);
        printf("PID: %d\n", history[i].pid);
        printf("Start time: %s\n", start_time_str);
        printf("Execution time: %.6f seconds\n", history[i].exec_time);
        printf("----------------------------\n");
    }
}

void displayHistory()
{
    // display command history
    printf("Command history:\n");
    for (int i = 0; i < history_count; i++) {
        printf("%d: %s\n", i + 1, history[i].command);
    }
}

void convertCmd(char *cmd_string, char **argv)
{
    // convert the command string to the required format
    char *ptr;
    i = 0;
    ptr = strtok(cmd_string, " ");
    while (ptr != NULL)
    {
        argv[i] = ptr;
        i++;
        ptr = strtok(NULL, " ");
    }

    argv[i] = NULL;
}

int executePipeCmd(char *cmds[], int num_pipes)
{
    // handles multiple piped commands
    int pipefd[MAX_PIPES][2];
    pid_t pids[MAX_PIPES + 1];
    int i;
    for (i = 0; i < num_pipes; i++)
    {
        if (pipe(pipefd[i]) == -1)
        {
            perror("pipe failed");
            return 1;
        }
    }

    for (i = 0; i <= num_pipes; i++)
    {
        pids[i] = fork();
        if (pids[i] < 0)
        {
            perror("fork failed");
            return 1;
        }

        if (pids[i] == 0)
        {
            if (i > 0)
            {
                dup2(pipefd[i - 1][0], STDIN_FILENO);
            }

            if (i < num_pipes)
            {
                dup2(pipefd[i][1], STDOUT_FILENO);
            }

            for (int j = 0; j < num_pipes; j++)
            {
                close(pipefd[j][0]);
                close(pipefd[j][1]);
            }

            char *argv[MAX_SIZE_ARG];
            convertCmd(cmds[i], argv);
            execvp(argv[0], argv);
            perror("exec failed");
            exit(EXIT_FAILURE);
        }
    }
    for (i = 0; i < num_pipes; i++)
    {
        close(pipefd[i][0]);
        close(pipefd[i][1]);
    }

    for (i = 0; i <= num_pipes; i++)
    {
        waitpid(pids[i], NULL, 0);
    }

    return 0;
}

void handleSigint(int sig)
{
    // signal handler to handle ctrl +c
    printf("\nCtrl+C detected. Printing history before exiting...\n");
    printFullHistory();
    exit(0);
}

void launch()
{
    // starts the shell
    while (1)
    {
        // get the command from user
        getCmd();

        // bypass empty commands
        if (!strcmp("", cmd))
        {
            continue;
        }

        // check for "exit" command
        if (!strcmp("exit", cmd))
        {
            printFullHistory();
            break;
        }

        // check for "history" command
        if (!strcmp("history", cmd))
        {
            displayHistory();
            continue;
        }

        // save command to history
        if (history_count < MAX_HISTORY)
        {
            strcpy(history[history_count].command, cmd);

            // record the start time
            gettimeofday(&history[history_count].start_time, NULL);
        }

        // split command into parts based on '|'
        char *cmds[MAX_PIPES + 1];
        int num_pipes = 0;
        cmds[num_pipes] = strtok(cmd, "|");
        while (cmds[num_pipes] != NULL)
        {
            num_pipes++;
            cmds[num_pipes] = strtok(NULL, "|");
        }

        if (num_pipes > 1)
        {
            // execute command with pipes
            executePipeCmd(cmds, num_pipes - 1);
        }
        else
        {
            // fit the command into *argv[]
            convertCmd(cmd, argv);

            // fork and execute the command
            pid = fork();
            if (-1 == pid)
            {
                printf("failed to create a child\n");
                continue;
            }
            else if (0 == pid)
            {
                // execute a command
                execvp(argv[0], argv);
                perror("Command execution failed");
                exit(EXIT_FAILURE);
            }
            else
            {
                // store the PID in history
                if (history_count < MAX_HISTORY)
                {
                    history[history_count].pid = pid;
                }

                // wait for the command to finish
                struct timeval end_time;
                if (NULL == argv[i])
                {
                    waitpid(pid, NULL, 0);

                    // record the end time and calculate execution time
                    gettimeofday(&end_time, NULL);
                    if (history_count < MAX_HISTORY)
                    {
                        history[history_count].exec_time =
                            (end_time.tv_sec - history[history_count].start_time.tv_sec) +
                            (end_time.tv_usec - history[history_count].start_time.tv_usec) / 1e6;
                        history_count++;
                    }
                }
            }
        }
    }
}

int main()
{
    // tie the handler to the SIGINT signal (Ctrl+C)
    signal(SIGINT, handleSigint);

    // start the shell
    launch();

    return 0;
}