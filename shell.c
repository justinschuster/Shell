#define _POSIX_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>

#define SH_RL_BUFSIZE 1024
#define SH_TOK_BUFSIZE 64
#define SH_TOK_DELIM " \t\r\n\a"

// Shell pid, gpid 
static pid_t SH_PID;
static pid_t SH_PGID;
pid_t pid;

struct sigaction act_child;
struct sigaction act_int;

// Global flags
int no_prompt;
int background;

/*
 * Function Declarations for builtin shell commands
 */
int shell_cd(char **args);
int shell_help(char **args);
int shell_quit(char **args);

// handler for SIGCHILD
void signalHandler_child(int p) {
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
    printf("\n");
}

// handler for SIGINT
void signalHandler_int(int p) {
    
    if (kill(pid, SIGTERM) == 0) {
        no_prompt = 1;
    } else {
        printf("\n");
    }
} 

/*
 * List of builtin commands, followed by their corresponding functions
 */
char *builtin_str[] = {
    "cd",
    "help",
    "quit",
};

int (*builtin_func[]) (char **) = {
    &shell_cd,
    &shell_help,
    &shell_quit,
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

// Check to see if & is at the end of final token but not alone in the token
// Returns 1 when we find the background flag
int check_background(char **args) {
    int arg_num = 0;  
    char *last_token;
   
    // get the number of arguments
    while (args[arg_num] != NULL) { 
        arg_num++;
    } 
    
    // Compare & with last character of last token then handle accordingly
    last_token = args[arg_num-1]; 
    if (last_token[strlen(last_token)-1] == '&') {
        last_token[strlen(last_token)-1] = '\0';
        return 1;
    } else {
        return 0;
    }
}

// Creates child processes to execute non builtin commands
int launch_shell(char **args) {
    pid_t wpid; 
    int status;
    
    if (!background) {
        background = check_background(args); 
    } 

    pid = fork();

    if (pid == 0) {
        // Child process   
           
        // Execute not builtin function
        if (execvp(args[0], args) == -1) {
            perror("shell");
        }

        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // Error forking
        perror("shell");
        exit(EXIT_FAILURE);
    } 
    
    // if background flag not set we wait for process to finish
    if (!background) {
        wpid = waitpid(pid, &status, WUNTRACED);
    }

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

/* TODO */
// Executes builtin functions or returns non builtin function calls
// Need to add redirection functionality
int shell_execute(char **args) {
    int i = 0;
    int j = 0;

    char *args_copy[256];

    if (args[0] == NULL) {
        // Empty command was entered
        return 1;
    }
   
    // Need to add ">" and "<" implementation 
    while (args[j] != NULL) {
        if ((strcmp(args[j], "&")) == 0) {
            break;
        }
        
        args_copy[j] = args[j];
        j++;
    } 

    // quit end shell 
    if (strcmp(args[0], "quit") == 0) return shell_quit(args);
    // help returns list of builtin functions
    else if (strcmp(args[0], "help") == 0) return shell_help(args);
    // cd changes the directory
    else if (strcmp(args[0], "cd") == 0) return shell_cd(args);     
    // args[0] is not a builtin function
    else {
        while (args[i] != NULL && background == 0) {
            if (strcmp(args[i], "&") == 0) {
                // Change background flag
                // & should be last thing in command line end loop
                background = 1;
                args[i] = NULL;
            }

            i++;
        }
    }
    args_copy[i] = NULL;

    // Prob should change this to args_copy[i]
    return launch_shell(args);
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

int main (int argc, char *argv[]) {
    
    no_prompt = 0;
    background = 0;

    pid = -10; // pid not possible

    SH_PID = getpid();

    act_child.sa_handler = signalHandler_child;
    act_int.sa_handler = signalHandler_int;

    sigaction(SIGCHLD, &act_child, 0);
    sigaction(SIGINT, &act_int, 0);

    // Create own process group
    setpgid(SH_PID, SH_PID); // shell process is group leader

    SH_PGID = getpgrp();
    if (SH_PID != SH_PGID) {
        perror("Shell is not process group leader");
        exit(EXIT_FAILURE);
    }
    
    if (argc == 1) {
        interactive_mode();
    } else {
        printf("\nYou tried to run shell with arguments but only interactive mode is avaiable at this time\n");
        printf("rerun shell with no arguments\n\n"); 
    } 
      

    return 0;
}
