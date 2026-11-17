#include "shell.h"
#include <fcntl.h>

/* Copy tokens into argv, removing "<" / ">" and their filenames.
   Returns 0 on success, -1 on syntax error. */
static int parse_redirects(char** tokens, char** argv, char** infile, char** outfile)
{
    int argc = 0;
    *infile = NULL;
    *outfile = NULL;

    for (int i = 0; tokens[i] != NULL; i++) {
        if (strcmp(tokens[i], "<") == 0 || strcmp(tokens[i], ">") == 0) {
            if (tokens[i + 1] == NULL) {
                fprintf(stderr, "syntax error: missing file after %s\n", tokens[i]);
                return -1;
            }
            if (tokens[i][0] == '<')
                *infile = tokens[i + 1];
            else
                *outfile = tokens[i + 1];
            i++;                              /* skip the filename */
        } else {
            argv[argc++] = tokens[i];
        }
    }
    argv[argc] = NULL;

    if (argc == 0) {
        fprintf(stderr, "syntax error: missing command\n");
        return -1;
    }
    return 0;
}

/* Runs in a child: set up < and > , then exec. Never returns. */
static void child_exec(char** tokens)
{
    char* argv[MAXARGS + 1];
    char* infile;
    char* outfile;

    if (parse_redirects(tokens, argv, &infile, &outfile) < 0)
        exit(1);

    if (infile != NULL) {
        int fd = open(infile, O_RDONLY);
        if (fd < 0) { perror(infile); exit(1); }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    if (outfile != NULL) {
        int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) { perror(outfile); exit(1); }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    execvp(argv[0], argv);
    perror("Command not found");     /* only runs if execvp fails */
    exit(1);
}

int execute(char** arglist, int background)
{
    int status;
    int pipe_at = -1;

    /* text shown by the jobs command */
    char job_text[128] = "";
    for (int i = 0; arglist[i] != NULL; i++) {
        if (i > 0)
            strncat(job_text, " ", sizeof(job_text) - strlen(job_text) - 1);
        strncat(job_text, arglist[i], sizeof(job_text) - strlen(job_text) - 1);
    }

    for (int i = 0; arglist[i] != NULL; i++) {
        if (strcmp(arglist[i], "|") == 0) {
            pipe_at = i;
            break;
        }
    }

    /* ---- no pipe: single command (with optional < and >) ---- */
    if (pipe_at == -1) {
        int cpid = fork();
        if (cpid == -1) {
            perror("fork failed");
            return -1;
        }
        if (cpid == 0)
            child_exec(arglist);
        if (background) {
            add_job(cpid, job_text);
            printf("[bg] pid %d\n", cpid);
        } else {
            waitpid(cpid, &status, 0);
        }
        return 0;
    }

    /* ---- one pipe: left | right ---- */
    char* left[MAXARGS + 1];
    for (int i = 0; i < pipe_at; i++)
        left[i] = arglist[i];
    left[pipe_at] = NULL;
    char** right = &arglist[pipe_at + 1];

    if (pipe_at == 0 || right[0] == NULL) {
        fprintf(stderr, "syntax error: missing command around |\n");
        return -1;
    }
    for (int i = 0; right[i] != NULL; i++) {
        if (strcmp(right[i], "|") == 0) {
            fprintf(stderr, "only one pipe is supported\n");
            return -1;
        }
    }

    int pfd[2];
    if (pipe(pfd) == -1) {
        perror("pipe");
        return -1;
    }

    int pid1 = fork();
    if (pid1 == -1) {
        perror("fork failed");
        close(pfd[0]);
        close(pfd[1]);
        return -1;
    }
    if (pid1 == 0) {                          /* left child: writer */
        dup2(pfd[1], STDOUT_FILENO);
        close(pfd[0]);
        close(pfd[1]);
        child_exec(left);
    }

    int pid2 = fork();
    if (pid2 == -1) {
        perror("fork failed");
        close(pfd[0]);
        close(pfd[1]);
        waitpid(pid1, &status, 0);
        return -1;
    }
    if (pid2 == 0) {                          /* right child: reader */
        dup2(pfd[0], STDIN_FILENO);
        close(pfd[0]);
        close(pfd[1]);
        child_exec(right);
    }

    close(pfd[0]);                            /* parent closes both ends */
    close(pfd[1]);
    if (background) {
        add_job(pid2, job_text);
        printf("[bg] pid %d\n", pid2);
        return 0;
    }
    waitpid(pid1, &status, 0);
    waitpid(pid2, &status, 0);
    return 0;
}
