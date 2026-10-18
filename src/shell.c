/*
 * Minish shell. This file is (proudly) human written, using Fedora Workstation 44 :-)
 *
 * Chatzikallias Panagiotis - ics24115 
 */

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

static char cwd[PATH_MAX];
static char oldcwd[PATH_MAX];
static struct termios orig_term;

// Our built in methods, dispatched
static const struct builtin b_ins[] = {
	{ "cd", 1, b_cd },
	{ "exit", 0, b_exit },
	{ "clear", 0, b_clear },
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
	setcwd();
	setup_terminal(); // changes tabs to be pure spaces, otherwise backspaces break the terminal

	// TODO: Install the required signal handlers here
	 
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
		jobs[i].active = 0;
	}
}

/**
 * @brief Adds a job to the array, in the first free index
 */
void add_bg_job(const pid_t pid)
{
	for (int i = 0; i < MAX_JOBS; i++) {
		// Find the first position in the array with active == 0 and add the
		// new job there. Then call break to exit the loop.
        // Add your code here...
	}
}

/** 
 * @brief Removes a completed job from the array
 */
void remove_bg_job(const pid_t pid)
{
	for (int i = 0; i < MAX_JOBS; i++) {
		// Find the first position in the array where its pid matches with the
		// pid parameter of the function and then clean it by setting pid to
		// -1 and active to 0.
        // Add your code here...
	}
}

/**
 * @brief Cleans up any completed jobs
 */
void reap_bg_jobs(void) {
	pid_t pid;
	int status;

	for (int i = 0; i < MAX_JOBS; i++) {
		// Call pid = waitpid() system call properly for each array slot.
		// If this call returns a positive pid, then this means that we just
		// found a terminated background process. Call remove_bg_job()
		// properly to remove this process from the jobs array.
          		
        // Add your code here...
	}	
}

/**
 * @brief Execute all commands in a parsed pipeline.\
 * @return 0 on success, -1 on error, -2 if exit was requested.
 */
static int execute_pipeline(struct pipeline *pipeline)
{
	pid_t *pids;	
	int result = 0;
	
	// Check for built ins
	if(pipeline->command_count == 1){
		struct command *cmd = &pipeline->commands[0];
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

	// No internal command found. We have to execute a new program for each
	// command in the pipeline.

	// Allocate an array to store the PID of each child process.
    // You may use the array below, if you find it helpful.    
	pids = calloc(pipeline->command_count, sizeof(*pids));
	if (pids == NULL)
		return -1;

	// Process each command in the pipeline. Remember that the command_count
	// tells us the number of commands in the pipeline.
	// E.g., cmd1 | cmd2 : 2 commands.
	// E.g., cmd1 : 1 command.

	// Add your code here...
        
	free(pids);
	return result;
}

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

	if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &orig_term) != 0)
		return;

	t = orig_term;
	t.c_oflag = (t.c_oflag & ~TABDLY) | TAB3;
	tcsetattr(STDIN_FILENO, TCSANOW, &t);
	atexit(restore_terminal);
}

/**
 * @brief Puts the terminal back the way we found it
 */
static void restore_terminal(void)
{
	tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);
}

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

/**
 * @brief My own addition. Can't have a shell without clear :D
 */
static int b_clear(struct command *cmd)
{
	system("clear"); // iffy but I am not about to mess with ANSI and terminal compatibility
	fflush(stdout);
	return 0;
}