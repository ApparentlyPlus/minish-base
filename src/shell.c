/*
 * Minish shell. This file is (proudly) human written, using Fedora Workstation 44 :-)
 *
 * Chatzikallias Panagiotis - ics24115 
 */

#define _DEFAULT_SOURCE

#include <linux/limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include "shell.h"

static struct job jobs[MAX_JOBS];
static char cwd[PATH_MAX];
static char oldcwd[PATH_MAX];
static struct termios oterm;

// Our built in methods, dispatched
static const struct builtin b_ins[] = {
	{ "cd", 1, b_cd },
	{ "exit", 0, b_exit },
};

/**
 * @brief Run the minish command interpreter.
 *
 * @return Process exit status.
 */
int main(void)
{	
	char line[INPUT_SIZE];
	struct pipeline pipeline = {0};
	int error, result;

	// Setup calls
	setcwd();
	setup_terminal(); // changes tabs to be pure spaces, otherwise backspaces break the terminal
	init_bg_jobs(jobs);

	// Our handlers
	reghandler(SIGCHLD, handle_sigchld, SA_RESTART); // clean finished background jobs
	reghandler(SIGINT, SIG_IGN, 0); // ignore CTRL + C in the parent shell
	 
	// The main shell loop
	for (;;) {
		// Print the prompt
		print_prompt();

		// Read a command line from the user
		result = read_line(line, sizeof(line));

		// Handle read errors
		if (result == 0)
			break;

		if (result < 0) {
			if (errno == EINTR)
				continue;

			perror("minish: Cannot read the command line.");
			return EXIT_FAILURE;
		}

		// Parse the line
		error = parse_line(line, &pipeline);

		// An error occurred during parsing
		if (error != PARSE_SUCCESS) {
			fprintf(stderr, "minish: %s\n", parse_error_string(error));
			continue;
		}

		// The user just pressed enter, but we ain't got anything to do
		if (pipeline.command_count == 0) {
			pipeline_free(&pipeline);
			continue;
		}

		// Execute the command pipeline
		if ((result = execute_pipeline(&pipeline)) == -2) {
			pipeline_free(&pipeline);
			break;
		}
		if (result < 0)
			perror("minish");

		// Clean up
		pipeline_free(&pipeline);
	}
	return EXIT_SUCCESS;
}

/**
 * @brief Handles completed signals for subprocesses
 */
void handle_sigchld(int signo){
	reap_bg_jobs(); // clean up anything completed
}

/**
 * @brief Registers a handler for a signal
 */
static int reghandler(int signo, void (*handler)(int), int flags)
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = flags;

	if (sigaction(signo, &sa, NULL) == -1) {
		perror("sigaction");
		return -1;
	}
	return 0;
}

/**
 * @brief Execute all commands in a parsed pipeline.\
 * @return 0 on success, -1 on error, -2 if exit was requested.
 */
static int execute_pipeline(struct pipeline *pipeline)
{
	int result = 0;
	pid_t* pids;
	
	// Check for built ins
	if(pipeline->command_count == 1){
		struct command* cmd = &pipeline->commands[0];
		const struct builtin *b = ffunc(cmd->argv[0]); // find the builtin func if it exists, else null

		if(b){
			// check if it has more args than it should
			if(cmd->argc - 1 > b->margs){
				if(b->margs == 0)
					fprintf(stderr, RED "[-] The command '%s' takes no arguments.\n" RESET, b->name);
				else
					fprintf(stderr, RED "[-] The command '%s' takes at most %zu argument(s).\n" RESET, b->name, b->margs);
				return 0;
			}
			return b->run(cmd);
		}
	}

	// Keep track of the pids launched -> we will need this
	pids = calloc(pipeline->command_count, sizeof(*pids));
	if (pids == NULL)
		return -1;

	// The idea here is that we launch all commands, then:
	//
	// 1.) If the whole pipeline is foreground, we wait for them to finish - by blocking. 
	// 2.) If not, we record them and let the signal handlers deal with them.

	size_t cnt;
	for(cnt = 0; cnt < pipeline->command_count; cnt++){
		struct command* cmd = &pipeline->commands[cnt];
		pid_t pid = fork();
		if(pid < 0){
			result = -1;
			break; // failed, wait and collect
		}

		// child process, descriptors remain intact
		else if(pid == 0){
			// handle CTRL + C normally INSIDE the subprocess
			if(pipeline->background != 1)
				reghandler(SIGINT, SIG_DFL, 0);
			
			execvp(cmd->argv[0], cmd->argv);
			perror(cmd->argv[0]); // only if exec fails
			_exit(127); // _exit instead of exit because of the tab handling thingy (this bit me hard)
		}

		pids[cnt] = pid; // record the pid
	}

	// if foreground, block and wait
	if(pipeline->background != 1){
		for(size_t i = 0; i < cnt; i++)
			waitpid(pids[i], NULL, 0);
	}

	// otherwise we put them in the jobs tracker for the handlers
	else
		for(size_t i = 0; i < cnt; i++)
			add_bg_job(pids[i]); // find and add it
        
	free(pids);
	return result;
}

#pragma region Misc

/**
 * @brief Sets the current working directory. Called on application startup.
 */
void setcwd(void){
   	if (getcwd(cwd, sizeof(cwd)) == NULL) {
		perror("Failed to set current working directory");
		exit(0);
	}
   	return;
}

/**
 * @brief Makes the terminal echo tabs as spaces.
 */
static void setup_terminal(void)
{
	struct termios t;

	if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &oterm) != 0)
		return;

	t = oterm;
	t.c_oflag = (t.c_oflag & ~TABDLY) | TAB3;
	tcsetattr(STDIN_FILENO, TCSANOW, &t);
	atexit(restore_terminal);
}

/**
 * @brief Puts the terminal back the way we found it
 */
static void restore_terminal(void)
{
	tcsetattr(STDIN_FILENO, TCSANOW, &oterm);
}

#pragma region Helpers

/**
 * @brief Print the interactive shell prompt.
 */
static void print_prompt(void)
{
	if (isatty(STDIN_FILENO) != 0) {
		printf(BOLD MAGENTA "minish" RESET BOLD ":" GREEN "%s" RESET "$ ", cwd);
		fflush(stdout);
	}
}

/**
 * @brief Read one command line from standard input.
 */
static int read_line(char *buffer, size_t size)
{
	if (fgets(buffer, size, stdin) == NULL) {
		if (feof(stdin) != 0)
			return 0;

		return -1;
	}

	buffer[strcspn(buffer, "\n")] = '\0';

	return 1;
}

/**
 * @brief Initializes the job array
 */
void init_bg_jobs(struct job *jobs)
{
	if (!jobs)
		return;
		
	for (int i = 0; i < MAX_JOBS; i++) {		
		jobs[i].pid = -1;
		jobs[i].active = false;
	}
}

/**
 * @brief Adds a job to the array, in the first free index
 */
void add_bg_job(const pid_t pid)
{
	for (int i = 0; i < MAX_JOBS; i++) {
		if(!jobs[i].active){
			jobs[i].pid = pid;
			jobs[i].active = true;
			break;
		}
	}
}

/** 
 * @brief Removes a completed job from the array
 */
void remove_bg_job(const pid_t pid)
{
	for (int i = 0; i < MAX_JOBS; i++) {
		if(jobs[i].pid == pid){
			jobs[i].pid = -1;
			jobs[i].active = false;
			break;
		}
	}
}

/**
 * @brief Cleans up any completed jobs
 */
void reap_bg_jobs(void) {
	pid_t pid;
	int status;

	for (int i = 0; i < MAX_JOBS; i++) {
		if(jobs[i].active && waitpid(jobs[i].pid, &status, WNOHANG) > 0){
			remove_bg_job(jobs[i].pid);
		}
	}	
}

#pragma region "Builtins"

/**
 * @brief Looks up a built in command by name
 * @returns The matching table entry, or NULL if it's not a built in
 */
static const struct builtin *ffunc(const char *name)
{
	for(size_t i = 0; i < sizeof(b_ins) / sizeof(b_ins[0]); i++)
		if(strcmp(name, b_ins[i].name) == 0)
			return &b_ins[i];
	return NULL;
}

/**
 * @brief cd. Not much to say here.
 */
static int b_cd(struct command *cmd)
{
	char buf[PATH_MAX];
	char *path = cmd->argc > 1 ? cmd->argv[1] : "~"; // I love idiomatic C like this

	if(strcmp(path, "-") == 0){
		if(oldcwd[0] == '\0'){
			fprintf(stderr, RED "[-] No previous directory\n" RESET);
			return 0;
		}
		path = oldcwd;
	}
	else if(path[0] == '~'){
		snprintf(buf, sizeof(buf), "%s%s", getenv("HOME"), path + 1);
		path = buf;
	}

	if(chdir(path) != 0){
		fprintf(stderr, RED "[-] %s: %s\n" RESET, path, strerror(errno));
		return 0;
	}
	strcpy(oldcwd, cwd);
	setcwd();
	return 0;
}

/**
 * @brief This just returns -2 because an exit was requested
 */
static int b_exit(struct command *cmd)
{
	return -2;
}

#pragma endregion