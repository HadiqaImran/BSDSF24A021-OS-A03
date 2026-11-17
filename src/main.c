#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_BLOCK 20

static char* skip_ws(char* s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    return s;
}

static void trim_end(char* s)
{
    int len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t'))
        s[--len] = '\0';
}

/* Run ONE command (no ';' inside). A trailing '&' means background.
   Returns the command's exit status (0 for built-ins and background jobs). */
static int run_command(char* cmd)
{
    trim_end(cmd);
    int len = strlen(cmd);

    int background = 0;
    if (len > 0 && cmd[len - 1] == '&') {
        background = 1;
        cmd[--len] = '\0';
    }

    char** arglist = tokenize(cmd);
    if (arglist == NULL)
        return 0;

    int status = 0;
    if (handle_builtin(arglist) == 0)
        status = execute(arglist, background);

    // Free the memory allocated by tokenize()
    for (int i = 0; arglist[i] != NULL; i++) {
        free(arglist[i]);
    }
    free(arglist);
    return status;
}

/* Split a line at each ';' and run the pieces one after another.
   Returns the status of the last command. */
static int run_line(char* line)
{
    int status = 0;
    char* rest = line;

    while (rest != NULL) {
        char* semi = strchr(rest, ';');
        if (semi != NULL)
            *semi = '\0';
        status = run_command(rest);
        rest = (semi != NULL) ? semi + 1 : NULL;
    }
    return status;
}

/* if <command> / then / ... / [else / ...] / fi */
static void handle_if(char* cmdline)
{
    char* cond = skip_ws(cmdline + 2);
    if (cond[0] == '\0') {
        fprintf(stderr, "syntax error: missing condition after if\n");
        return;
    }
    cond = strdup(cond);

    char* then_lines[MAX_BLOCK];
    char* else_lines[MAX_BLOCK];
    int then_n = 0, else_n = 0;
    int state = 0;      /* 0 = waiting for then, 1 = then-block, 2 = else-block */
    int ok = 1;

    while (1) {
        char* line = readline("> ");
        if (line == NULL) {
            fprintf(stderr, "syntax error: unexpected end of input (missing fi)\n");
            ok = 0;
            break;
        }

        char* t = skip_ws(line);
        trim_end(t);

        if (t[0] == '\0') {
            /* blank line: ignore */
        } else if (strcmp(t, "fi") == 0) {
            if (state == 0) {
                fprintf(stderr, "syntax error: missing then\n");
                ok = 0;
            }
            free(line);
            break;
        } else if (strcmp(t, "then") == 0 && state == 0) {
            state = 1;
        } else if (strcmp(t, "else") == 0 && state == 1) {
            state = 2;
        } else if (state == 1 && then_n < MAX_BLOCK) {
            then_lines[then_n++] = strdup(t);
        } else if (state == 2 && else_n < MAX_BLOCK) {
            else_lines[else_n++] = strdup(t);
        } else {
            fprintf(stderr, "syntax error near: %s\n", t);
            ok = 0;
        }
        free(line);
    }

    if (ok) {
        int status = run_line(cond);              /* run the if-command */
        char** block = (status == 0) ? then_lines : else_lines;
        int n = (status == 0) ? then_n : else_n;
        for (int i = 0; i < n; i++)
            run_line(block[i]);
    }

    free(cond);
    for (int i = 0; i < then_n; i++)
        free(then_lines[i]);
    for (int i = 0; i < else_n; i++)
        free(else_lines[i]);
}

int main() {
    char* cmdline;

    while (1) {
        reap_jobs();                       /* collect finished background jobs */

        cmdline = readline(PROMPT);
        if (cmdline == NULL)
            break;

        if (cmdline[0] == '\0') {
            free(cmdline);
            continue;
        }

        /* !n : replace the line with the n-th history command */
        if (cmdline[0] == '!') {
            char* old = get_history(atoi(cmdline + 1));
            if (old == NULL) {
                fprintf(stderr, "history: no such command\n");
                free(cmdline);
                continue;
            }
            char* replaced = strdup(old);
            printf("%s\n", replaced);
            free(cmdline);
            cmdline = replaced;
        }

        add_history(cmdline);        /* readline's history (arrow keys) */
        add_to_history(cmdline);     /* our own history (history, !n)   */

        if (strncmp(cmdline, "if", 2) == 0 &&
            (cmdline[2] == ' ' || cmdline[2] == '\t' || cmdline[2] == '\0'))
            handle_if(cmdline);
        else
            run_line(cmdline);

        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
