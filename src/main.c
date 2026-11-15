#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>

int main() {
    char* cmdline;
    char** arglist;

    while ((cmdline = readline(PROMPT)) != NULL) {

        /* skip empty lines */
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

        if ((arglist = tokenize(cmdline)) != NULL) {
            if (handle_builtin(arglist) == 0)
                execute(arglist);

            // Free the memory allocated by tokenize()
            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }
            free(arglist);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
