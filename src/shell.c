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
        memset(arglist[i], 0, ARGLEN);
    }

    char* cp = cmdline;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++; // Skip leading whitespace

        if (*cp == '\0') break; // Line was only whitespace

        int len = 0;
        int in_quotes = 0;
        while (*cp != '\0' && (in_quotes || (*cp != ' ' && *cp != '\t'))) {
            if (*cp == '"') {
                in_quotes = !in_quotes;      /* quotes group words, they are not kept */
            } else if (len < ARGLEN - 1) {
                arglist[argnum][len++] = *cp;
            }
            cp++;
        }
        arglist[argnum][len] = '\0';
        argnum++;
    }

    if (argnum == 0) { // No arguments were parsed
        for (int i = 0; i < MAXARGS + 1; i++) free(arglist[i]);
        free(arglist);
        return NULL;
    }

    for (int i = argnum; i < MAXARGS + 1; i++)   // free the unused buffers
        free(arglist[i]);
    arglist[argnum] = NULL;
    return arglist;
}

int handle_builtin(char** arglist)
{
    if (arglist == NULL || arglist[0] == NULL)
        return 0;

    if (is_assignment(arglist[0])) {
        if (arglist[1] != NULL) {
            fprintf(stderr, "syntax error: put values with spaces in quotes, e.g. NAME=\"a b\"\n");
            return 1;
        }
        char* eq = strchr(arglist[0], '=');
        *eq = '\0';
        set_var(arglist[0], eq + 1);
        return 1;
    }

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
        printf("  set       - show all shell variables (assign with NAME=value)\n");
        return 1;
    }

    if (strcmp(arglist[0], "history") == 0) {
        print_history();
        return 1;
    }

    if (strcmp(arglist[0], "set") == 0) {
        print_vars();
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

/* ---------- shell variables (linked list) ---------- */
typedef struct var {
    char* name;
    char* value;
    struct var* next;
} var_t;

static var_t* var_head = NULL;

/* "NAME=..." where NAME is letters, digits or '_' and does not start with a digit */
int is_assignment(const char* s)
{
    if (s == NULL || !(isalpha((unsigned char)s[0]) || s[0] == '_'))
        return 0;
    int i = 0;
    while (isalnum((unsigned char)s[i]) || s[i] == '_')
        i++;
    return s[i] == '=';
}

void set_var(const char* name, const char* value)
{
    var_t* last = NULL;
    for (var_t* v = var_head; v != NULL; v = v->next) {
        if (strcmp(v->name, name) == 0) {      /* already exists: update */
            free(v->value);
            v->value = strdup(value);
            return;
        }
        last = v;
    }
    var_t* v = (var_t*) malloc(sizeof(var_t));  /* new node at the end */
    v->name = strdup(name);
    v->value = strdup(value);
    v->next = NULL;
    if (last == NULL)
        var_head = v;
    else
        last->next = v;
}

const char* get_var(const char* name)
{
    for (var_t* v = var_head; v != NULL; v = v->next)
        if (strcmp(v->name, name) == 0)
            return v->value;
    return NULL;
}

void print_vars(void)
{
    for (var_t* v = var_head; v != NULL; v = v->next)
        printf("%s=%s\n", v->name, v->value);
}

/* Replace every argument of the form $NAME with a fresh copy of its value. */
void expand_variables(char** arglist)
{
    for (int i = 0; arglist[i] != NULL; i++) {
        if (arglist[i][0] == '$' && arglist[i][1] != '\0') {
            const char* value = get_var(arglist[i] + 1);
            char* expanded = strdup(value != NULL ? value : "");
            free(arglist[i]);
            arglist[i] = expanded;
        }
    }
}
