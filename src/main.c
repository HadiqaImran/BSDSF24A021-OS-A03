#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>

/* Run ONE command (no ';' inside). A trailing '&' means background. */
static void run_command(char* cmd)
{
    int len = strlen(cmd);
    while (len > 0 && (cmd[len - 1] == ' ' || cmd[len - 1] == '\t'))
        cmd[--len] = '\0';

    int background = 0;
    if (len > 0 && cmd[len - 1] == '&') {
        background = 1;
        cmd[--len] = '\0';
    }

    char** arglist = tokenize(cmd);
    if (arglist == NULL)
        return;

    if (handle_builtin(arglist) == 0)
        execute(arglist, background);

    // Free the memory allocated by tokenize()
    for (int i = 0; arglist[i] != NULL; i++) {
        free(arglist[i]);
    }
    free(arglist);
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

        /* split the line at each ';' and run the pieces one after another */
        char* rest = cmdline;
        while (rest != NULL) {
            char* semi = strchr(rest, ';');
            if (semi != NULL)
                *semi = '\0';
            run_command(rest);
            rest = (semi != NULL) ? semi + 1 : NULL;
        }

        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
