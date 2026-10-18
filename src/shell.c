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
#include <stdio.h>
#include "shell.h"

/**
 * @brief Run the minish command interpreter.
 *
 * @return Process exit status.
 */
int main(void)
{	
	char cwd[PATH_MAX];
	char line[INPUT_SIZE];
	struct pipeline pipeline = {0};
	int error, result;
	setcwd(cwd, sizeof(cwd));

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
		if ((result = execute_pipeline(&pipeline)) < 0)
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
		(void)printf("minish$ ");
		(void)fflush(stdout);
	}
}

/**
 * @brief Read one command line from standard input.
 *
 * @param[out] buffer Destination buffer.
 * @param[in] size Size of the destination buffer.
 *
 * @return 1 when a line was read, 0 on EOF, or -1 on error.
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
 * @brief Execute all commands in a parsed pipeline.
 *
 * @param[in] pipeline Parsed pipeline.
 *
 * @return 0 on success, -1 on error, -2 if exit was requested.
 */
static int execute_pipeline(const struct pipeline *pipeline)
{
	pid_t *pids;	
	int result = 0;
	
	// First, we have to handle the internal commands. To do this, we need to
	// check if our command count is one, and if the name of the command matches
	// an internal command. Remember that the command name is always stored in
	// argv[0].

	// Add your code here...

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
void setcwd(char* cwd, size_t size){
   	if (getcwd(cwd, size) == NULL) {
		perror("Failed to set current working directory");
		exit(0);
	}
   	return;
}
