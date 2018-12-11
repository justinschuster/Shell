#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <termios.h>

#define SH_RL_BUFSIZE 1024
#define SH_TOK_BUFSIZE 64
#define SH_TOK_DELIM " \t\r\n\a"

/* A process object is a single process */
typedef struct process {
    struct process *next;   /* next process in the pipeline */
    char **argv;            /*for exec */
    pid_t pid;              /* process ID */
    char completed;         /* true if process has completed */
    char stopped;           /* true if process has stopped */
    int status;             /* reported status value */
} process;

/* A job is a pipeline of process */
typedef struct job {
    struct job *next;           /* next active job */
    char *command;              /* command line, used for messages */
    process *first_process;     /* list of processes in this job */
    pid_t pgid;                 /* process group ID */
    char notified;              /* true if user told about stopped job */
    struct termios tmodes;      /* saved terminal modes */
    int stdin, stdout, stderr;  /* standard i/o channels */
} job;

/*
 * List of builtin commands, followed by their corresponding functions
 */
char *builtin_str[] = {
    "barrier",
    "cd",
    "help",
    "quit",
};

int shell_num_builtins() {
    return sizeof(builtin_str) / sizeof(char *);
}

/*
 * Builtin function implementations
 */
// Changes Directory like normal cd
int shell_cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "shell: expected argument to \"cd\"\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("shell");
        }
    }

    return 1;
}

/* TODO */
// Displays the builtin functions
// Probably should add more information about each function
int shell_help(char **args) {
    int i;

    printf("\nHELP\n");
    printf("Use the following builtin functions:\n\n");

    // Print name of builtin functions
    for (i = 0; i < shell_num_builtins(); i++) {
        printf(" %s\n", builtin_str[i]);
    }

    printf("\n");

    return 1; 
}

// Quits shell obviously
int shell_quit(char **args) {
    return 0;
}

int shell_barrier(char **args) {
    return 1; 
}

// Reads in line from shell so we can parse it
char *read_line(void) {
    int buff_size = SH_RL_BUFSIZE;
    int position = 0;
    char *buffer = malloc(sizeof(char) *buff_size);
    int c;

    if(!buffer) {
        fprintf(stderr, "shell: allocation error\n");
        exit(EXIT_FAILURE);
    }

    // Read a character
    while (1) {
        c = getchar();

        // IF EOF replace with a null char and return
        if (c == EOF || c == '\n') {
            buffer[position] = '\0';
            return buffer;
        } else {
            buffer[position] = c;
        }
        position++;

        // If we have exceeded the buffer, reallocate
        if (position >= buff_size) {
            buff_size += SH_RL_BUFSIZE;
            buffer = realloc(buffer, buff_size);

            if (!buffer) {
                fprintf(stderr, "shell: allocate error\n");
                exit(EXIT_FAILURE);
            }
        }
    }


// Parses line into tokens. Prob could give it more descriptive name
char **split_line(char (*line)) {
    int buff_size = SH_TOK_BUFSIZE, position = 0;
    char **tokens = malloc(buff_size * sizeof(char *));
    char *token;
    

    if (!tokens) {
        fprintf(stderr, "shell: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, SH_TOK_DELIM);
    while (token != NULL) {
        tokens[position] = token; 
        position++;

        if (position >= buff_size) {
            buff_size += SH_TOK_BUFSIZE;
            tokens = realloc(tokens, buff_size * sizeof(char *));

            if (!tokens) {
                fprintf(stderr, "shell: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, SH_TOK_DELIM);
    }

    tokens[position] = NULL; 

    return tokens;
}

// Interactive Mode's main loop
void interactive_mode(void) {
    char *line;
    char **args;
    int status;

    do {
        printf("prompt> ");
        line = read_line();
        args = split_line(line); 
        status = shell_execute(args);

        free(line);
        free (args);
        background = 0;
    } while (status);
}

/* Set up shell */
void init_shell() {
   /* See if we are running interactively */
    shell_terminal = STDIN_FILENO;
    shell_is_interactive = isatty(shell_terminal);

    if (shell_is_interactive) {
        /* Loop until in foreground */
        while (tcgetpgrp(shell_terminal) != (SH_PGID = getpgrp())) {
            kill (- SH_PGID, SIGTTIN);
        }

        /* Ignore interactive and job-control signals */
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        signal(SIGCHLD, SIG_IGN);

        // Put ourselves in our own process group 
        SH_PGID = getpid();
        if (setpgid(SH_PGID, SH_PGID) < 0) {
            perror("Couldn't put shell in it's own process group");
            exit(1);
        }

        /* Grab contro of the terminal */
        tcsetpgrp(shell_terminal, SH_PGID);

        // Save default attributes
        tcgetattr(shell_terminal, &shell_tmodes);
    }
    
}

int main (int argc, char *argv[]) {
	init_shell();

	interactive_mode;
}