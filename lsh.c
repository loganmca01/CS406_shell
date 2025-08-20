/* Logan McAllister
 * CS406 Project 1
 * Professor Pfaffmann
 * 3/2/24
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>

/* arbitrary number of paths for creating array */
#define MAX_PATHS 10

/* buffer size for getline */
#define INPUT_SIZE 256

/* variables for paths and error string */
char *paths[MAX_PATHS];
int num_paths;
char error_message[30] = "An error has occurred\n";

int run_line(char *line);
void initialize_default_path();

int exec_command(char *c);
void exec_cd(char *filename);
void exec_pwd();
void exec_path(char *args);
void exec_external_setup(char *command, char *args);
void exec_external_run(char *command_name, char *command_filepath, char *args, char *output_file);

int get_exec_filepath(char *buff, char *command);
void print_paths();
void free_paths();

/* these are global so file can be closed in child process created in functions other than main */
FILE *input_source;
int mode; /* 0 for batch, 1 for interactive */

int main(int argc, char *argv[]) {

    initialize_default_path();

    /* get input source (batch mode or interactive) */
    if (argc == 1)
    {
        input_source = stdin;
        mode = 1;
        printf("lsh> ");
    }
    else if (argc == 2)
    {
        input_source = fopen(argv[1], "r");
        if (!input_source) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
        mode = 0;
    }
    else
    {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }

    /* getline requires pointer to pointer, so array created then pointer to first element created */
    char in[INPUT_SIZE];
    char *input = &in[0];
    size_t size = sizeof(char) * INPUT_SIZE;

    while (getline(&input, &size, input_source) > 0)
    {

        /* check for end of file without exit to make sure it doesn't loop forever */
        int eof_check = 0;
        if (input[strlen(input) - 1] != '\n') {
            eof_check = 1;
        }

        /* replace newline character with null terminator */
        input[strcspn(input, "\r\n")] = '\0';

        int done = run_line(input);

        if ((!mode && eof_check) || done) break;
        if (mode) printf("lsh> ");

    }

    if (!mode) fclose(input_source);
    free_paths();

    return 0;
}

void initialize_default_path() {

    paths[0] = strdup("/bin");
    num_paths = 1;

}

/* run individual lines from input,
 * breaks down parallel processes then calls exec_command for each */
int run_line(char *line) {

    char *mask = line;
    int done = 0;

    strsep(&mask, "&");

    /* fork here if mask not null, otherwise just execute in main process */
    if (!mask) done = exec_command(line);
    else
    {
        int pid = fork();

        if (pid < 0)
        {
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
        else if (pid == 0)
        {
            exec_command(line);
            if (!mode) fclose(input_source);
            free_paths();
            exit(0);
        }
    }

    /* keep forking each parallel process */
    while (mask)
    {
        char *mask2 = mask;
        strsep(&mask2, "&");

        int pid = fork();

        if (pid < 0)
        {
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
        else if (pid == 0)
        {
            exec_command(mask);
            if (!mode) fclose(input_source);
            free_paths();
            exit(0);
        }

        mask = mask2;
    }

    /* wait for all children to finish, unsure if there's a prettier way to do this */
    while (wait(NULL) > 0);

    return done;

}

/* execute each individual command after parallel processes separated by fork.
 * called by each process.
 */
int exec_command(char *c) {

    while (c && *c == ' ') c++;
    if (!*c) return 0;

    char *mask = c;
    strsep(&mask, " ");

    while (mask && *mask == ' ') mask++;

    /* check if command is built-in or outside function, call appropriate function */
    if (!strcmp(c, "exit"))
    {
        if (mask && *mask)
        {
            write(STDERR_FILENO, error_message, strlen(error_message));
            return 0;
        }
        return 1;
    }
    if (!strcmp(c, "cd"))
    {
        exec_cd(mask);
    }
    /* Added pwd because I found the man page for getcwd and liked it for early testing.
     * Commented out for submission.
     */
    /*
    else if (!strcmp(c, "pwd"))
    {
        if (mask)
        {
            write(STDERR_FILENO, error_message, strlen(error_message));
            return 0;
        }
        exec_pwd();
    }
    */
    else if (!strcmp(c, "path"))
    {
        exec_path(mask);
    }
    else
    {
        exec_external_setup(c, mask);
    }

    return 0;


}

/* exec cd built-in function. calls chdir on passed filename */
void exec_cd(char *filename) {

    if (!filename)
    {
        write(STDERR_FILENO, error_message, strlen(error_message));
        return;
    }

    char *check = filename;

    strsep(&check, " ");

    if (check)
    {
        write(STDERR_FILENO, error_message, strlen(error_message));
        return;
    }

    /* try to change into new directory */
    if (chdir(filename))
    {
        write(STDERR_FILENO, error_message, strlen(error_message));
        return;
    }

}

/* ignore for submission, just used for early testing of cd
void exec_pwd() {

    char cwd[PATH_MAX];
    if (!getcwd(cwd, PATH_MAX))
    {
        write(STDERR_FILENO, error_message, strlen(error_message));
        return;
    }
    printf("%s\n", cwd);

}
*/

/* execute built-in path function. verifies that each of the new paths can
 * be accessed, then adds them to paths global.
 */
void exec_path(char *args) {
    free_paths();

    int count = 0;

    /* for each path in command arguments, try to add to paths */
    while (args && *args && count < MAX_PATHS)
    {

        char *mask = args;
        strsep(&mask, " ");

        /* access returns 0 if file can be accessed */
        if (!access(args, F_OK))
        {
            paths[count++] = strdup(args);
        }
        else
        {
            write(STDERR_FILENO, error_message, strlen(error_message));
            return;
        }

        while (mask && *mask == ' ')
        {
            mask++;
        }

        args = mask;
    }

}

/* set up for execution of external command */
void exec_external_setup(char *command, char *args) {

    /* command path buffer */
    char command_path[1024];

    /* calls function that puts filepath for execution into command_path if valid path found */
    if (!get_exec_filepath(command_path, command)) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        return;
    }

    char *output = args;

    strsep(&output, ">");

    /* error checking for second attempted redirect or multiple files in redirect */
    if (output)
    {
        while (*output == ' ') output++;

        char *mask = output;
        strsep(&mask, ">");
        if (mask)
        {
            write(STDERR_FILENO, error_message, strlen(error_message));
            return;
        }

        mask = output;
        strsep(&mask, " ");
        while (mask && *mask == ' ') mask++;

        if (mask && *mask)
        {
            write(STDERR_FILENO, error_message, strlen(error_message));
            return;
        }
    }

    exec_external_run(command, &command_path[0], args, output);

}

void exec_external_run(char *command_name, char *command_filepath, char *args, char *output_file) {

    int pid = fork();

    if (pid < 0)
    {
        write(STDERR_FILENO, error_message, strlen(error_message));
        return;
    }
    else if (pid == 0)
    {

        /* if an output file is specified, set stdout and stderr to it */
        if (output_file)
        {
            int fd = open(output_file, O_CREAT | O_TRUNC | O_RDWR, S_IRWXU);

            /* verify that file can be opened, and duplicated to stdout and stderr.
             * if not, free all active memory, close files and exit process */
            if (fd < 0)
            {
                if (!mode) fclose(input_source);
                free_paths();
                write(STDERR_FILENO, error_message, strlen(error_message));
                exit(1);
            }
            if (dup2(fd, STDOUT_FILENO) < 0)
            {
                if (!mode) fclose(input_source);
                free_paths();
                close(fd);
                write(STDERR_FILENO, error_message, strlen(error_message));
                exit(1);
            }
            if (dup2(fd, STDERR_FILENO) < 0)
            {
                if (!mode) fclose(input_source);
                free_paths();
                close(fd);
                write(STDERR_FILENO, error_message, strlen(error_message));
                exit(1);
            }
            close(fd);
        }

        char *arr[32];

        arr[0] = command_name;

        int count = 1;

        /* fill array of char* for exec call with all arguments */
        while (args && *args)
        {
            char *mask = args;

            strsep(&mask, " ");

            arr[count++] = args;

            while (mask && *mask == ' ')
            {
                mask++;
            }

            args = mask;

        }

        /* arrays passed to exec must be null terminated */
        arr[count] = NULL;

        /* free memory and close file before exec call, as successful exec will never return here */
        if (!mode) fclose(input_source);
        free_paths();

        execv(command_filepath, arr);

        /* only reachable if exec fails */
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);

    }
    else
    {
        waitpid(pid, NULL, 0);
    }

}

/* get filepath for external command execution */
int get_exec_filepath(char *buff, char *command) {

    /* check for access in current directory */
    if (!access(command, X_OK))
    {
        strcpy(buff, command);
        return 1;
    }

    /* check for access appending to each path */
    for (int i = 0; i < MAX_PATHS; i++)
    {
        /* once null path is found, no need to check rest */
        if (!paths[i]) break;

        char check[1024];

        /* concatenate command name to end of path */
        strcpy(check, paths[i]);
        strcat(check, "/");
        strcat(check, command);

        /* copy into buffer if successful path found */
        if (!access(check, X_OK))
        {
            strcpy(buff, check);
            return 1;
        }
    }

    return 0;

}

/* used for testing path built-in */
void print_paths() {
    for (int i = 0; i < MAX_PATHS; i++)
    {

        if (paths[i])
        {
            printf("%s\n", paths[i]);
        }
        else return;

    }
}

/* iterate through and free path char*s in array */
void free_paths() {

    for (int i = 0; i < MAX_PATHS; i++)
    {
        if (paths[i])
        {
            free(paths[i]);
            paths[i] = NULL;
        }
    }

}