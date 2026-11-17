#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_LEN 512
#define MAXARGS 10
#define ARGLEN 30
#define PROMPT "FCIT> "
#define HISTORY_SIZE 20
#define MAX_JOBS 20

// Function prototypes
char* read_cmd(char* prompt, FILE* fp);
char** tokenize(char* cmdline);
int execute(char** arglist, int background);
void add_job(int pid, const char* name);
void reap_jobs(void);
void print_jobs(void);
int handle_builtin(char** arglist);
void add_to_history(const char* cmdline);
void print_history(void);
char* get_history(int n);

#endif // SHELL_H
