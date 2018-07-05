#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h> // open()

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
	cmd_fun_t *fun;
	char *cmd;
	char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
	{cmd_help, "?", "show this help menu"},
	{cmd_exit, "exit", "exit the command shell"},
	{cmd_pwd, "pwd", "print the working directory."},
	{cmd_cd, "cd", "change directory to target."},
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens *tokens) {
	for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
		printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
	return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens *tokens) {
	exit(0);
}

/* NEW! Print working directory */
int cmd_pwd(unused struct tokens *tokens) {
	char cwd[256];
	if (getcwd(cwd, sizeof(cwd)) == NULL)
		perror("getcwd() error");
	else
		fprintf(stdout, "%s \n", cwd);
	return 1;
}

/* NEW! Change directory */
int cmd_cd(unused struct tokens *tokens) {
	if (tokens_get_length(tokens) != 2) {
		printf("Number of tokens must be 2!\n");	
	} else if (chdir(tokens_get_token(tokens, 1)) != 0) {
		printf("Fail to change directory! \n");
	}
	return 1;	
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
	for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
		if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
			return i;
	return -1;
}

/* New! Check if the substring " < " is found in the command */
int check_in(struct tokens *tokens) {
	int tk_length = tokens_get_length(tokens);
	for (int i=0; i<tk_length; i++) {
		if (!strcmp(tokens_get_token(tokens, i), "<")) return i;
	}
	return -1;
}

int check_out(struct tokens *tokens) {
	int tk_length = tokens_get_length(tokens);
	for (int i=0; i<tk_length; i++) {
		if (!strcmp(tokens_get_token(tokens, i), ">")) return i;
	}
	return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
	/* Our shell is connected to standard input. */
	shell_terminal = STDIN_FILENO;

	/* Check if we are running interactively */
	shell_is_interactive = isatty(shell_terminal);

	if (shell_is_interactive) {
		/* If the shell is not currently in the foreground, we must pause the shell until it becomes a
		 * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
		 * foreground, we'll receive a SIGCONT. */
		while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
			kill(-shell_pgid, SIGTTIN);

		/* Saves the shell's process id */
		shell_pgid = getpid();

		/* Take control of the terminal */
		tcsetpgrp(shell_terminal, shell_pgid);

		/* Save the current termios to a variable, so it can be restored later. */
		tcgetattr(shell_terminal, &shell_tmodes);
	}
}

void shell_exec(struct tokens *tokens) {
	/* REPLACE this to run commands as programs. */
	// fprintf(stdout, "No built-in commands. Run as programs.\n");

	/* NEW! Run the program! */
	int tk_length = tokens_get_length(tokens);
	char * args[tk_length+1];
	for (int i=0; i<tk_length; i++) {
		args[i] = tokens_get_token(tokens, i);
	}
	args[tk_length] = NULL;

	if (!fork()) {	// Child process
		fprintf(stdout, "Program start... \n");

		if (execv(args[0], args) == -1) { // direct execute fails

			char* env_path_str = getenv("PATH");
			//printf("%s\n", env_path_str);
			char* delim = ":";

			char* env_path_token = strtok(env_path_str, delim);
			// Notice! Allocate memory for copying string
			char path_arg[500];
			strcpy(path_arg, env_path_token);
			strcat(path_arg, "/");
			strcat(path_arg, args[0]);

			while (env_path_token != NULL) {
				//printf("%s\n", path_arg);
				if (execv(path_arg, args) == -1) { // fails to execute
					env_path_token = strtok(NULL, delim);
					strcpy(path_arg, env_path_token);
					strcat(path_arg, "/");
					strcat(path_arg, args[0]);
				} else {
					printf("Works fine!\n");
					break;
				}
			}
		}
		exit(0);
	} else {  // Parent process
		wait(NULL);
		fprintf(stdout, "Program finished.\n");
	}
}
int main(unused int argc, unused char *argv[]) {
	init_shell();

	static char line[4096];
	int line_num = 0;

	/* Please only print shell prompts when standard input is not a tty */
	if (shell_is_interactive)
		fprintf(stdout, "%d: ", line_num);

	while (fgets(line, 4096, stdin)) {
		/* Split our line into words. */
		struct tokens *tokens = tokenize(line);
		int tk_length = tokens_get_length(tokens);
		int ptk;

		/* Find which built-in function to run. */
		int fundex = lookup(tokens_get_token(tokens, 0));

		if (fundex >= 0) { // built-in function
			cmd_table[fundex].fun(tokens);

		} else if ((ptk=check_in(tokens))>=0) { // contains " < "
			printf("check_in. tk_length:%d ptk:%d\n", tk_length, ptk);
			if (ptk == 0 || ptk != tk_length-2) {
				printf("Position of \"<\" is not correct.\n");
			}


		} else if ((ptk = check_out(tokens))>=0) { // contains " > "
			printf("check_out. tk_length:%d ptk:%d\n", tk_length, ptk);
			if (ptk == 0 || ptk != tk_length-2) {
				printf("Position of \">\" is not correct.\n");
			}

			if (!fork()) { // Child process
				fprintf(stdout, "Out to file start... \n");
				int fd = open(tokens_get_token(tokens, tk_length-1), O_WRONLY);
				if(fd < 0)    return 1;

				//Now we redirect standard output to the file using dup2
				if(dup2(fd,1) < 0) { // fileno(stdout) is 1
					close(fd);
					return 1; // this can return 0 normally
				}
				//Now standard out has been redirected, we can write to the file

				//At the end the file has to be closed:
				close(fd);
				exit(0);
			} else { // Parent process
				wait(NULL);
				fprintf(stdout, "Out to file finish. \n");
			}


		} else {
			shell_exec(tokens);
		}
		if (shell_is_interactive)
			/* Please only print shell prompts when standard input is not a tty */
			fprintf(stdout, "%d: ", ++line_num);

		/* Clean up memory */
		tokens_destroy(tokens);
	}

	return 0;
}
