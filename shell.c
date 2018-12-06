#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define SH_RL_BUFSIZE 1024
#define SH_TOK_BUFSIZE 64
#define SH_TOK_DELIM " \t\r\n\a"

/*
 * Function Declarations for builtin shell commands
 */
int shell_cd(char **args);
int shell_help(char **args);
int shell_quit(char **args);


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

int shell_help(char **args) {
    int i;

    printf("\nHELP\n");
    printf("Use the following builtin functions:\n\n");

    for (i = 0; i < shell_num_builtins(); i++) {
        printf(" %s\n", builtin_str[i]);
    }

    printf("\n");

    return 1; 
}

int shell_quit(char **args) {
    return 0;
}

int launch_shell(char **args) {
    pid_t pid, wpid;
    int status;
    int background = 0;

    pid = fork();
    if (pid == 0) {
        // Child process 
            
        if (execvp(args[0], args) == -1) {
            perror("shell");
        }

        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // Error forking
        perror("shell");
    } 

    if (!background) {
        wpid = waitpid(pid, &status, WUNTRACED);
    }

    return 1;
}

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

int shell_execute(char **args) {
    int i;

    if (args[0] == NULL) {
        // Empty command was entered
        return 1;
    }

    for (i = 0; i < shell_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }

    return launch_shell(args);
}

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
    } while (status);
}

int main (int argc, char *argv[]) {
    
    if (argc == 1) {
        interactive_mode();
    } else {
        printf("\nYou tried to run shell with arguments but only interactive mode is avaiable at this time\n");
        printf("rerun shell with no arguments\n\n"); 
    } 
      

    return 0;
}
