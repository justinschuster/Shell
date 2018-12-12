#define _XOPEN_SOURCE 1000

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <pwd.h>
#include <glob.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>

#define NR_JOBS 20
#define PATH_BUFSIZE 1024
#define COMMAND_BUFSIZE 1024
#define TOKEN_BUFSIZE 64
#define TOKEN_DELIMITERS " \t\r\n\a"

#define BACKGROUND_EXECUTION 0
#define FOREGROUND_EXECUTION 1
#define PIPELINE_EXECUTION 2

#define COMMAND_EXTERNAL 0
#define COMMAND_QUIT 1
#define COMMAND_CD 2
#define COMMAND_BARRIER 3

#define PROC_FILTER_ALL 0
#define PROC_FILTER_DONE 1
#define PROC_FILTER_REMAINING 2

#define STATUS_RUNNING 0
#define STATUS_DONE 1
#define STATUS_SUSPENDED 2
#define STATUS_CONTINUED 3
#define STATUS_TERMINATED 4

const char* STATUS_STRING[] = {
    "running",
    "done",
    "suspended",
    "continued",
    "terminated"
};

struct process {
    char *command;
    int argc;
    char **argv;
    char *input_path;
    char *output_path;
    pid_t pid;
    int type;
    int status;
    struct process *next;
};

struct job {
    int id;
    struct process *root;
    char *command;
    pid_t pgid;
    int mode;
};

struct shell_info {
    char cur_user[TOKEN_BUFSIZE];
    char cur_dir[PATH_BUFSIZE];
    char pw_dir[PATH_BUFSIZE];
    struct job *jobs[NR_JOBS + 1];
};

// Shell object were we store all info about shell
struct shell_info *shell;

// Variable to save terminal modes
struct termios shell_tmodes;

// Gets the job id based of the process'es id
int get_job_id(int pid) {
    int i;
    struct process *proc;

    for (i = 1; i <= NR_JOBS; i++) {
        if (shell->jobs[i] != NULL) {
            for (proc = shell->jobs[i]->root; proc != NULL; proc = proc->next) {
                if (proc->pid == pid) {
                    return i;
                }
            }
        }
    }

    return -1;
}

struct job* get_job_by_id(int id) {
    if (id > NR_JOBS) {
        return NULL;
    }

    return shell->jobs[id];
}

int get_pgid_by_job_id(int id) {
    struct job *job = get_job_by_id(id);

    if (job == NULL) {
        return -1;
    }

    return job->pgid;
}

// Gets pathname of current working directory 
void update_cwd_info() {
    getcwd(shell->cur_dir, sizeof(shell->cur_dir));
}

// Prints the shell prompt
void display_prompt() {
    printf("prompt> ");
}

// Handles SIGINT signals
void sigint_handler(int signal) {
    printf("\n");
}

int get_proc_count(int id, int filter) {
    if (id > NR_JOBS || shell->jobs[id] == NULL) {
        return -1;
    }

    int count = 0;
    struct process *proc;
    for (proc = shell->jobs[id]->root; proc != NULL; proc = proc->next) {
        if (filter == PROC_FILTER_ALL ||
            (filter == PROC_FILTER_DONE && proc->status == STATUS_DONE) ||
            (filter == PROC_FILTER_REMAINING && proc->status != STATUS_DONE)) {
            count++;
        }
    }

    return count;
}

int get_next_job_id() {
    int i;

    for (i = 1; i <= NR_JOBS; i++) {
        if (shell->jobs[i] == NULL) {
            return i;
        }
    }

    return -1;
}

// Frees the pointers in a job object
int release_job(int id) {
    if (id > NR_JOBS || shell->jobs[id] == NULL) {
        return -1;
    }

    struct job *job = shell->jobs[id];
    struct process *proc, *tmp;
    for (proc = job->root; proc != NULL; ) {
        tmp = proc->next;
        free(proc->command);
        free(proc->argv);
        free(proc->input_path);
        free(proc->output_path);
        free(proc);
        proc = tmp;
    }

    free(job->command);
    free(job);

    return 0;
}

// Sets satus for process
int set_process_status(int pid, int status) {
    int i;
    struct process *proc;

    for (i = 1; i <= NR_JOBS; i++) {
        if (shell->jobs[i] == NULL) {
            continue;
        }
        for (proc = shell->jobs[i]->root; proc != NULL; proc = proc->next) {
            if (proc->pid == pid) {
                proc->status = status;
                return 0;
            }
        }
    }

    return -1;
}

int wait_for_pid(int pid) {
    int status = 0;

    waitpid(pid, &status, WUNTRACED);
    if (WIFEXITED(status)) {
        set_process_status(pid, STATUS_DONE);
    } else if (WIFSIGNALED(status)) {
        set_process_status(pid, STATUS_TERMINATED);
    } else if (WSTOPSIG(status)) {
        status = -1;
        set_process_status(pid, STATUS_SUSPENDED);
    }

    return status;
}

// Let's us know if job is completed
int job_completed_check(int id) {
    if (id > NR_JOBS || shell->jobs[id] == NULL) {
        return 0;
    }

    struct process *proc;
    for (proc = shell->jobs[id]->root; proc != NULL; proc = proc->next) {
        if (proc->status != STATUS_DONE) {
            return 0;
        }
    }

    return 1;
}

int print_processes_of_job(int id) {
    if (id > NR_JOBS || shell->jobs[id] == NULL) {
        return -1;
    }

    printf("[%d]", id);

    struct process *proc;
    for (proc = shell->jobs[id]->root; proc != NULL; proc = proc->next) {
        printf(" %d", proc->pid);
    }
    printf("\n");

    return 0;
}

// Displays the job status in the command line
int print_job_status(int id) {
    if (id > NR_JOBS || shell->jobs[id] == NULL) {
        return -1;
    }

    printf("[%d]", id);

    struct process *proc;
    for (proc = shell->jobs[id]->root; proc != NULL; proc = proc->next) {
        printf("\t%d\t%s\t%s", proc->pid,
            STATUS_STRING[proc->status], proc->command);
        if (proc->next != NULL) {
            printf("|\n");
        } else {
            printf("\n");
        }
    }

    return 0;
}

int wait_for_job(int id) {
    if (id > NR_JOBS || shell->jobs[id] == NULL) {
        return -1;
    }

    int proc_count = get_proc_count(id, PROC_FILTER_REMAINING);
    int wait_pid = -1, wait_count = 0;
    int status = 0;

    do {
        wait_pid = waitpid(-shell->jobs[id]->pgid, &status, WUNTRACED);
        wait_count++;

        if (WIFEXITED(status)) {
            set_process_status(wait_pid, STATUS_DONE);
        } else if (WIFSIGNALED(status)) {
            set_process_status(wait_pid, STATUS_TERMINATED);
        } else if (WSTOPSIG(status)) {
            status = -1;
            set_process_status(wait_pid, STATUS_SUSPENDED);
            if (wait_count == proc_count) {
                print_job_status(id);
            }
        }
    } while (wait_count < proc_count);

    return status;
}

int insert_job(struct job *job) {
    int id = get_next_job_id();

    if (id < 0) {
        return -1;
    }

    job->id = id;
    shell->jobs[id] = job;
    return id;
}

// Removes job if completed
int remove_job(int id) {
    if (id > NR_JOBS || shell->jobs[id] == NULL) {
        return -1;
    }

    release_job(id);
    shell->jobs[id] = NULL;

    return 0;
}

// Let's us know if process is a zombie process
void check_zombie() {
    int status, pid;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        // Process has exited
        if (WIFEXITED(status)) {
            set_process_status(pid, STATUS_DONE);
        // Process is currently suspended
        } else if (WIFSTOPPED(status)) {
            set_process_status(pid, STATUS_SUSPENDED);
        // Process is still running
        } else if (WIFCONTINUED(status)) {
            set_process_status(pid, STATUS_CONTINUED);
        }

        int job_id = get_job_id(pid);
        if (job_id > 0 && job_completed_check(job_id)) {
            print_job_status(job_id);
            remove_job(job_id);
        }
    }
}

// Changes the directory of the shell
int shell_cd(int argc, char** argv) {
    if (argc == 1) {
        chdir(shell->pw_dir);
        update_cwd_info();
        return 0;
    }

    if (chdir(argv[1]) == 0) {
        update_cwd_info();
        return 0;
    } else {
        printf("mysh: cd %s: No such file or directory\n", argv[1]);
        return 0;
    }
}

// Quits the shell
int shell_quit() {
    exit(0);
}

int shell_barrier() {
    int i;

     for (i = 1; i <= NR_JOBS; i++) {
        if (shell->jobs[i] != NULL) {
            wait_for_job(i); 
        }
    }

    return 0;
}

// Checks to see if commands are builtin
int get_command_type(char *command) {
    if (strcmp(command, "quit") == 0) {
        return COMMAND_QUIT;
    } else if (strcmp(command, "cd") == 0) {
        return COMMAND_CD;
    } else if (strcmp(command, "barrier") == 0) {
        return COMMAND_BARRIER;
    } else {
        return COMMAND_EXTERNAL;
    }
}

char* whitespace_strtrim(char* line) {
    char *head = line, *tail = line + strlen(line);

    while (*head == ' ') {
        head++;
    }
    while (*tail == ' ') {
        tail--;
    }
    *(tail + 1) = '\0';

    return head;
}

int execute_builtin_command(struct process *proc) {
    int status = 1;

    switch (proc->type) {
        case COMMAND_QUIT:
            shell_quit();
            break;
        case COMMAND_CD:
            shell_cd(proc->argc, proc->argv);
            break;
        case COMMAND_BARRIER:
            shell_barrier(proc->pid);
            break; 
        default:
            status = 0;
            break;
    }

    return status;
}

int launch_process(struct job *job, struct process *proc, int in_fd, int out_fd, int mode) {
    proc->status = STATUS_RUNNING;
    if (proc->type != COMMAND_EXTERNAL && execute_builtin_command(proc)) {
        return 0;
    }

    pid_t childpid;
    int status = 0;

    childpid = fork();

    if (childpid < 0) {
        return -1;
    } else if (childpid == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        proc->pid = getpid();
        if (job->pgid > 0) {
            setpgid(0, job->pgid);
        } else {
            job->pgid = proc->pid;
            setpgid(0, job->pgid);
        }

        if (in_fd != 0) {
            dup2(in_fd, 0);
            close(in_fd);
        }

        if (out_fd != 1) {
            dup2(out_fd, 1);
            close(out_fd);
        }

        if (execvp(proc->argv[0], proc->argv) < 0) {
            printf("shell: %s: command not found\n", proc->argv[0]);
            exit(0);
        }

        exit(0);
    } else {
        proc->pid = childpid;
        if (job->pgid > 0) {
            setpgid(childpid, job->pgid);
        } else {
            job->pgid = proc->pid;
            setpgid(childpid, job->pgid);
        }

        if (mode == FOREGROUND_EXECUTION) {
            tcsetpgrp(0, job->pgid);
            status = wait_for_job(job->id);
            signal(SIGTTOU, SIG_IGN);
            tcsetpgrp(0, getpid());
            signal(SIGTTOU, SIG_DFL);
        } 
    }

    return status;
}

int launch_job(struct job *job) {
    struct process *proc;
    int status = 0, in_fd = 0, fd[2], job_id = -1;

    check_zombie();
    if (job->root->type == COMMAND_EXTERNAL) {
        job_id = insert_job(job);
    }

    for (proc = job->root; proc != NULL; proc = proc->next) {
        if (proc == job->root && proc->input_path != NULL) {
            in_fd = open(proc->input_path, O_RDONLY);
            if (in_fd < 0) {
                printf("mysh: no such file or directory: %s\n", proc->input_path);
                remove_job(job_id);
                return -1;
            }
        }
        if (proc->next != NULL) {
            pipe(fd);
            status = launch_process(job, proc, in_fd, fd[1], PIPELINE_EXECUTION);
            close(fd[1]);
            in_fd = fd[0];
        } else {
            int out_fd = 1;
            if (proc->output_path != NULL) {
                out_fd = open(proc->output_path, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
                if (out_fd < 0) {
                    out_fd = 1;
                }
            }
            status = launch_process(job, proc, in_fd, out_fd, job->mode);
        }
    }

    if (job->root->type == COMMAND_EXTERNAL) {
        if (status >= 0 && job->mode == FOREGROUND_EXECUTION) {
            remove_job(job_id);
        } else if (job->mode == BACKGROUND_EXECUTION) {
            //print_processes_of_job(job_id);
        }
    }

    return status;
}

struct process *create_process(char *segment) {
    int bufsize = TOKEN_BUFSIZE;
    int position = 0;
    char *command = strdup(segment);
    char *token;
    char **tokens = (char**) malloc(bufsize * sizeof(char*));

    if (!tokens) {
        fprintf(stderr, "mysh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(segment, TOKEN_DELIMITERS);
    while (token != NULL) {
        glob_t glob_buffer;
        int glob_count = 0;
        if (strchr(token, '*') != NULL || strchr(token, '?') != NULL) {
            glob(token, 0, NULL, &glob_buffer);
            glob_count = glob_buffer.gl_pathc;
        }

        if (position + glob_count >= bufsize) {
            bufsize += TOKEN_BUFSIZE;
            bufsize += glob_count;
            tokens = (char**) realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "mysh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        if (glob_count > 0) {
            int i;
            for (i = 0; i < glob_count; i++) {
                tokens[position++] = strdup(glob_buffer.gl_pathv[i]);
            }
            globfree(&glob_buffer);
        } else {
            tokens[position] = token;
            position++;
        }

        token = strtok(NULL, TOKEN_DELIMITERS);
    }

    int i = 0, argc = 0;
    char *input_path = NULL, *output_path = NULL;
    while (i < position) {
        if (tokens[i][0] == '<' || tokens[i][0] == '>') {
            break;
        }
        i++;
    }
    argc = i;

    for (; i < position; i++) {
        if (tokens[i][0] == '<') {
            if (strlen(tokens[i]) == 1) {
                input_path = (char *) malloc((strlen(tokens[i + 1]) + 1) * sizeof(char));
                strcpy(input_path, tokens[i + 1]);
                i++;
            } else {
                input_path = (char *) malloc(strlen(tokens[i]) * sizeof(char));
                strcpy(input_path, tokens[i] + 1);
            }
        } else if (tokens[i][0] == '>') {
            if (strlen(tokens[i]) == 1) {
                output_path = (char *) malloc((strlen(tokens[i + 1]) + 1) * sizeof(char));
                strcpy(output_path, tokens[i + 1]);
                i++;
            } else {
                output_path = (char *) malloc(strlen(tokens[i]) * sizeof(char));
                strcpy(output_path, tokens[i] + 1);
            }
        } else {
            break;
        }
    }

    for (i = argc; i <= position; i++) {
        tokens[i] = NULL;
    }

    struct process *new_proc = (struct process*) malloc(sizeof(struct process));
    new_proc->command = command;
    new_proc->argv = tokens;
    new_proc->argc = argc;
    new_proc->input_path = input_path;
    new_proc->output_path = output_path;
    new_proc->pid = -1;
    new_proc->type = get_command_type(tokens[0]);
    new_proc->next = NULL;
    return new_proc;
}

struct job *shell_parse_command(char *line) {
    line = whitespace_strtrim(line);
    char *command = strdup(line);

    struct process *root_proc = NULL, *proc = NULL;
    char *line_cursor = line, *c = line, *seg;
    int seg_len = 0, mode = FOREGROUND_EXECUTION;

    if (line[strlen(line) - 1] == '&') {
        mode = BACKGROUND_EXECUTION;
        line[strlen(line) - 1] = '\0';
    }

    while (1) {
        if (*c == '\0' || *c == '|') {
            seg = (char*) malloc((seg_len + 1) * sizeof(char));
            strncpy(seg, line_cursor, seg_len);
            seg[seg_len] = '\0';

            struct process* new_proc = create_process(seg);
            if (!root_proc) {
                root_proc = new_proc;
                proc = root_proc;
            } else {
                proc->next = new_proc;
                proc = new_proc;
            }

            if (*c != '\0') {
                line_cursor = c;
                while (*(++line_cursor) == ' ');
                c = line_cursor;
                seg_len = 0;
                continue;
            } else {
                break;
            }
        } else {
            seg_len++;
            c++;
        }
    }

    struct job *new_job = (struct job*) malloc(sizeof(struct job));
    new_job->root = root_proc;
    new_job->command = command;
    new_job->pgid = -1;
    new_job->mode = mode;
    return new_job;
}

// Reads in line from shell so we can parse it
char *read_line(void) {
    int buff_size = COMMAND_BUFSIZE;
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
            buff_size += COMMAND_BUFSIZE;
            buffer = realloc(buffer, buff_size);

            if (!buffer) {
                fprintf(stderr, "shell: allocate error\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

// Loop for interactive mode, reads in from command line then executes commands
void interactive_mode() {
    char *line;
    struct job *j;
    int status = 1;

    while (1) {
        display_prompt();

        line = read_line();
        if (strlen(line) == 0) {
            check_zombie();
            continue;
        }

        j = shell_parse_command(line);
        status = launch_job(j);
    }
}

// Initilizes signals, process groups, and shell info
void shell_init() {
    // assign signal handler for SIGINT
    struct sigaction sigint_action = {
        .sa_handler = &sigint_handler,
        .sa_flags = 0
    };
    sigemptyset(&sigint_action.sa_mask);
    sigaction(SIGINT, &sigint_action, NULL);

    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    pid_t pid = getpid();
    setpgid(pid, pid);
    tcsetpgrp(0, pid);

    shell = (struct shell_info*) malloc(sizeof(struct shell_info));
    getlogin_r(shell->cur_user, sizeof(shell->cur_user));

    struct passwd *pw = getpwuid(getuid());
    strcpy(shell->pw_dir, pw->pw_dir);

    int i;
    for (i = 0; i < NR_JOBS; i++) {
        shell->jobs[i] = NULL;
    }

    update_cwd_info();

    // Update defualt terminal modes
    tcgetattr(0, &shell_tmodes);
}

int main(int argc, char **argv) {

    shell_init(); 
    interactive_mode();

	return EXIT_SUCCESS;
}