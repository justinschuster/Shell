#include <stdio.h>
#include <stdlib.h>

#define SH_RL_BUFSIZE 1024

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

char **split_line(char (*line));
int shell_execute(char **args);

void shell_loop(void) {
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
    
    shell_loop(); 
      

    return 0;
}
