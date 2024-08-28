#include "defs.h"
#include "types.h"
#include "readline.h"
#include "runcmd.h"

int ALTERNATIVE_STACK_SIZE = 1024 * 10;

char prompt[PRMTLEN] = { 0 };

// runs a shell command
static void
run_shell()
{
	char *cmd;

	while ((cmd = read_line(prompt)) != NULL) {
		if (run_cmd(cmd) == EXIT_SHELL)
			return;
	}
}

static void
handler_function()
{
	int status, pid_child;

	while ((pid_child = waitpid(0, &status, WNOHANG)) > 0) {
#ifndef SHELL_NO_INTERACTIVE
		if (isatty(1)) {
			fprintf(stdout,
			        "%s	Program terminated [pid: %d] with "
			        "status: %d %s\n",
			        COLOR_BLUE,
			        pid_child,
			        status,
			        COLOR_RESET);
		}
#endif
	}
}

// initializes the shell
// with the "HOME" directory
static void *
init_shell()
{
	stack_t stack;
	struct sigaction sigact;

	stack.ss_sp = malloc(ALTERNATIVE_STACK_SIZE);
	if (stack.ss_sp == NULL) {
		perror("error while allocating memory to alternative stack");
		exit(EXIT_FAILURE);
	}

	stack.ss_size = ALTERNATIVE_STACK_SIZE;
	stack.ss_flags = 0;
	if (sigaltstack(&stack, NULL) == -1) {
		perror("error while defining a new alternative stack for "
		       "signals");
		free(stack.ss_sp);
		exit(EXIT_FAILURE);
	}

	sigact.sa_flags = SA_ONSTACK | SA_RESTART;
	sigact.sa_handler = handler_function;
	sigemptyset(&sigact.sa_mask);
	if (sigaction(SIGCHLD, &sigact, NULL) == -1) {
		perror("error while setting signal handler for SIGCHLD");
		free(stack.ss_sp);
		exit(EXIT_FAILURE);
	}

	char buf[BUFLEN] = { 0 };
	char *home = getenv("HOME");

	if (chdir(home) < 0) {
		snprintf(buf, sizeof buf, "cannot cd to %s ", home);
		perror(buf);
	} else {
		snprintf(prompt, sizeof prompt, "(%s)", home);
	}

	return stack.ss_sp;
}

int
main(void)
{
	void *stack_space = init_shell();

	run_shell();

	free(stack_space);

	return 0;
}
