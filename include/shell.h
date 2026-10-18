
#pragma once
#include "parser.h"
#include <stdbool.h>

#define INPUT_SIZE 4096
#define MAX_JOBS 128

struct job {
	pid_t pid;
	bool active;
};

static void print_banner(void);
static void print_prompt(void);
static int read_line(char *buffer, size_t size);
static int execute_pipeline(struct pipeline *pipeline);
static int reghandler(int signo, void (*handler)(int), int flags);
static void setup_terminal(void);
static void restore_terminal(void);

void handle_sigchld(int signo);
void init_bg_jobs(struct job *jobs);
void add_bg_job(const pid_t pid);
void remove_bg_job(const pid_t pid);
void reap_bg_jobs(void);
void setcwd(void);

struct builtin {
	const char *name;
	size_t margs;
	int (*run)(struct command *cmd); // cmd dispatch
};

static const struct builtin *ffunc(const char *name);
static int b_cd(struct command *cmd);
static int b_exit(struct command *cmd);
static int b_about(struct command *cmd);

// Colors for style!

#define BLACK "\x1b[30m"
#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define BLUE "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN "\x1b[36m"
#define WHITE "\x1b[37m"
#define RESET "\x1b[0m"
#define BOLD "\x1b[1m"
#define DIM "\x1b[2m"
