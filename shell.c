#include <stdio.h>
#include <stdlib.h>

char *read_line(void);
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
