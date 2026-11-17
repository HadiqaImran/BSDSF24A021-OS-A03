#include "shell.h"

char* read_cmd(char* prompt, FILE* fp) {
    printf("%s", prompt);
    char* cmdline = (char*) malloc(sizeof(char) * MAX_LEN);
    int c, pos = 0;

    while ((c = getc(fp)) != EOF) {
        if (c == '\n') break;
        cmdline[pos++] = c;
    }

    if (c == EOF && pos == 0) {
        free(cmdline);
        return NULL; // Handle Ctrl+D
    }
    
    cmdline[pos] = '\0';
    return cmdline;
}

char** tokenize(char* cmdline) {
    // Edge case: empty command line
    if (cmdline == NULL || cmdline[0] == '\0' || cmdline[0] == '\n') {
        return NULL;
    }

    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    for (int i = 0; i < MAXARGS + 1; i++) {
        arglist[i] = (char*)malloc(sizeof(char) * ARGLEN);
        bzero(arglist[i], ARGLEN);
    }

    char* cp = cmdline;
    char* start;
    int len;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++; // Skip leading whitespace
        
        if (*cp == '\0') break; // Line was only whitespace

        start = cp;
        len = 1;
        while (*++cp != '\0' && !(*cp == ' ' || *cp == '\t')) {
            len++;
        }
        strncpy(arglist[argnum], start, len);
        arglist[argnum][len] = '\0';
        argnum++;
    }

    if (argnum == 0) { // No arguments were parsed
        for(int i = 0; i < MAXARGS + 1; i++) free(arglist[i]);
        free(arglist);
        return NULL;
    }

    arglist[argnum] = NULL;
    return arglist;
}
int handle_builtin(char** arglist)
{
    if (arglist == NULL || arglist[0] == NULL)
        return 0;

    if (strcmp(arglist[0], "exit") == 0) {
        printf("Exiting shell...\n");
        exit(0);
    }

    if (strcmp(arglist[0], "cd") == 0) {
        if (arglist[1] == NULL)
            fprintf(stderr, "cd: missing directory\n");
        else if (chdir(arglist[1]) != 0)
            perror("cd");
        return 1;
    }

    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  cd <dir>  - change directory\n");
        printf("  exit      - exit the shell\n");
        printf("  help      - show this message\n");
        printf("  jobs      - list running background jobs\n");
        printf("  history   - show command history (use !n to re-run)\n");
        return 1;
    }

    if (strcmp(arglist[0], "history") == 0) {
        print_history();
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        print_jobs();
        return 1;
    }

    return 0;
}

static char* history[HISTORY_SIZE];
static int history_count = 0;

void add_to_history(const char* cmdline)
{
    if (cmdline == NULL || cmdline[0] == '\0')
        return;
    if (history_count == HISTORY_SIZE) {      /* full: drop the oldest */
        free(history[0]);
        for (int i = 1; i < HISTORY_SIZE; i++)
            history[i - 1] = history[i];
        history_count--;
    }
    history[history_count++] = strdup(cmdline);
}

void print_history(void)
{
    for (int i = 0; i < history_count; i++)
        printf("%d  %s\n", i + 1, history[i]);
}

char* get_history(int n)
{
    if (n < 1 || n > history_count)
        return NULL;
    return history[n - 1];
}

/* ---------- background job list ---------- */
static int jobs_pid[MAX_JOBS];
static char jobs_text[MAX_JOBS][128];
static int job_count = 0;

void add_job(int pid, const char* name)
{
    if (job_count == MAX_JOBS) {
        fprintf(stderr, "too many background jobs\n");
        return;
    }
    jobs_pid[job_count] = pid;
    strncpy(jobs_text[job_count], name, 127);
    jobs_text[job_count][127] = '\0';
    job_count++;
}

/* Reap finished children without blocking, and drop them from the list. */
void reap_jobs(void)
{
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < job_count; i++) {
            if (jobs_pid[i] == pid) {
                printf("[done] pid %d  %s\n", (int)pid, jobs_text[i]);
                for (int j = i + 1; j < job_count; j++) {
                    jobs_pid[j - 1] = jobs_pid[j];
                    strcpy(jobs_text[j - 1], jobs_text[j]);
                }
                job_count--;
                break;
            }
        }
    }
}

void print_jobs(void)
{
    reap_jobs();
    if (job_count == 0) {
        printf("No background jobs.\n");
        return;
    }
    for (int i = 0; i < job_count; i++)
        printf("[%d] pid %d  %s\n", i + 1, jobs_pid[i], jobs_text[i]);
}
