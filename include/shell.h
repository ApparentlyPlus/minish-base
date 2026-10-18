
#pragma once
#include "parser.h"

#define INPUT_SIZE 4096
#define MAX_JOBS 128

struct job {
	pid_t pid;
	int active;
};

static struct job jobs[MAX_JOBS];

static void print_prompt(void);
static int read_line(char *buffer, size_t size);
static int execute_pipeline(const struct pipeline *pipeline);
static void init_bg_jobs(struct job *jobs);
static void add_bg_job(const pid_t pid);
static void remove_bg_job(const pid_t pid);
static void reap_bg_jobs(void);
static void setcwd(char* cwd, size_t size);
