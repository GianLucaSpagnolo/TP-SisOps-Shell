#include "exec.h"

#define EXECERROR -1
#define EXECSUCCESS 0

#define READ_FILE_ERROR -1
#define REDIRECT_IN 0
#define REDIRECT_OUT 1
#define REDIRECT_ERR 2
#define REDIRECT_ERR_OUT 3

#define PIPE_READ 0
#define PIPE_WRITE 1


static void
free_alt_stack()
{
	stack_t ss;
	if (sigaltstack(NULL, &ss) == -1) {
		perror("error while setting free the previous alternative "
		       "stack");
		_exit(1);
	}
	free(ss.ss_sp);
}

// sets "key" with the key part of "arg"
// and null-terminates it
//
// Example:
//  - KEY=value
//  arg = ['K', 'E', 'Y', '=', 'v', 'a', 'l', 'u', 'e', '\0']
//  key = "KEY"
//
static void
get_environ_key(char *arg, char *key)
{
	int i;
	for (i = 0; arg[i] != '='; i++)
		key[i] = arg[i];

	key[i] = END_STRING;
}

// sets "value" with the value part of "arg"
// and null-terminates it
// "idx" should be the index in "arg" where "=" char
// resides
//
// Example:
//  - KEY=value
//  arg = ['K', 'E', 'Y', '=', 'v', 'a', 'l', 'u', 'e', '\0']
//  value = "value"
//
static void
get_environ_value(char *arg, char *value, int idx)
{
	size_t i, j;
	for (i = (idx + 1), j = 0; i < strlen(arg); i++, j++)
		value[j] = arg[i];

	value[j] = END_STRING;
}

// sets the environment variables received
// in the command line
//
// Hints:
// - use 'block_contains()' to
// 	get the index where the '=' is
// - 'get_environ_*()' can be useful here
static void
set_environ_vars(char **eargv, int eargc)
{
	for (int i = 0; i < eargc; i++) {
		char key[FNAMESIZE];
		char value[FNAMESIZE];

		get_environ_key(eargv[i], key);
		get_environ_value(eargv[i], value, block_contains(eargv[i], '='));

		if (setenv(key, value, 1) == -1)
			perror("error while setting environment variable");
	}
}

static int
get_redir_flags(int redir_type)
{
	int flags = 0;
	switch (redir_type) {
	case REDIRECT_IN:
		flags = O_RDONLY | O_CLOEXEC;
		break;
	case REDIRECT_ERR:
		flags = O_CREAT | O_RDWR | O_CLOEXEC;
		break;
	default:
		flags = O_CREAT | O_RDWR | O_TRUNC | O_CLOEXEC;
	}
	return flags;
}

// opens the file in which the stdin/stdout/stderr
// flow will be redirected, and returns
// the file descriptor
//
// Find out what permissions it needs.
// Does it have to be closed after the execve(2) call?
//
// Hints:
// - if O_CREAT is used, add S_IWUSR and S_IRUSR
// 	to make it a readable normal file
static int
open_redir_fd(char *file, int redirect_case)
{
	int mode = 0;

	if (redirect_case != REDIRECT_IN) {
		mode = S_IWUSR | S_IRUSR;
	}

	int fd = open(file, get_redir_flags(redirect_case), mode);

	if (fd == -1) {
		_exit(READ_FILE_ERROR);
	}
	return fd;
}

static int
redirect_case(struct execcmd *r)
{
	if (strlen(r->in_file) > 0) {
		return REDIRECT_IN;
	} else if (strlen(r->err_file) > 0) {
		if ((strlen(r->err_file) >= 2) &&
		    (r->err_file[0] == '&' && r->err_file[1] == '1')) {
			return REDIRECT_ERR_OUT;
		}
		return REDIRECT_ERR;

	} else if (strlen(r->out_file) > 0) {
		return REDIRECT_OUT;
	}

	return -1;
}

static char *
get_redir_file(struct execcmd *r)
{
	int redir_type = redirect_case(r);

	char *file = NULL;

	switch (redir_type) {
	case REDIRECT_IN:
		file = r->in_file;
		break;
	case REDIRECT_ERR:
		file = r->err_file;
		break;
	default:
		file = r->out_file;
		break;
	}

	return file;
}
static int
dup_redir_std(int fd, struct execcmd *r)
{
	int redir_type = redirect_case(r);
	int state = 0;

	switch (redir_type) {
	case REDIRECT_IN:
		state = dup2(fd, STDIN_FILENO);
		break;
	case REDIRECT_OUT:
		state = dup2(fd, STDOUT_FILENO);
		break;
	case REDIRECT_ERR:
		state = dup2(fd, STDERR_FILENO);
		break;
	case REDIRECT_ERR_OUT:
		state = dup2(fd, STDOUT_FILENO);
		if (state == -1)
			break;
		state = dup2(fd, STDERR_FILENO);
		break;
	}

	if (state == -1) {
		perror("error while duplicating file descriptors");
		free_alt_stack();
		_exit(EXECERROR);
	}
	return fd;
}

// executes a command - does not return
//
// Hint:
// - check how the 'cmd' structs are defined
// 	in types.h
// - casting could be a good option
void
exec_cmd(struct cmd *cmd)
{
	// To be used in the different cases
	struct execcmd *e;
	struct backcmd *b;
	struct execcmd *r;
	struct pipecmd *p;

	switch (cmd->type) {
	case EXEC:
		e = (struct execcmd *) cmd;
		set_environ_vars(e->eargv, e->eargc);

		if (execvp(e->argv[0], e->argv) == EXECERROR) {
			perror("error while executing command");
			free_alt_stack();
			free_command(cmd);
			_exit(EXECERROR);
		}

		free_command(cmd);
		free_alt_stack();
		_exit(EXECSUCCESS);

	case BACK: {
		b = (struct backcmd *) cmd;
		e = (struct execcmd *) b->c;

		if (execvp(e->argv[0], e->argv) == EXECERROR) {
			perror("error while executing backtrack command");
			free_alt_stack();
			free_command(cmd);
			_exit(EXECERROR);
		}

		break;
	}

	case REDIR: {
		r = (struct execcmd *) cmd;

		int fd;
		fd = open_redir_fd(get_redir_file(r), redirect_case(r));
		dup_redir_std(fd, r);
		close(fd);

		if (execvp(r->argv[0], r->argv) == EXECERROR) {
			perror("error while executing redirection command");
			free_alt_stack();
			free_command(cmd);
			_exit(EXECERROR);
		}

		free_command(cmd);
		free_alt_stack();
		_exit(EXECSUCCESS);
	}

	case PIPE: {
		p = (struct pipecmd *) cmd;

		int pipe_cmd[2];
		pid_t left_pid, right_pid;
		int status = pipe(pipe_cmd);

		if (status == -1) {
			perror("pipe generation failed");
			close(pipe_cmd[PIPE_READ]);
			close(pipe_cmd[PIPE_WRITE]);
			free_command(p->leftcmd);
			free_command(p->rightcmd);
			free_command(parsed_pipe);
			free_alt_stack();
			_exit(EXECERROR);
		}

		left_pid = fork();

		if (left_pid < 0) {
			perror("error while forking pipe process");
			close(pipe_cmd[PIPE_READ]);
			close(pipe_cmd[PIPE_WRITE]);
			free_command(p->leftcmd);
			free_command(p->rightcmd);
			free_command(cmd);
			free_alt_stack();
			_exit(EXECERROR);
		}

		if (left_pid == 0) {
			free_command(p->rightcmd);
			close(pipe_cmd[PIPE_READ]);
			dup2(pipe_cmd[PIPE_WRITE], STDOUT_FILENO);
			close(pipe_cmd[PIPE_WRITE]);

			exec_cmd(p->leftcmd);
		} else {
			right_pid = fork();

			if (right_pid < 0) {
				perror("error while forking pipe process");
				close(pipe_cmd[PIPE_READ]);
				close(pipe_cmd[PIPE_WRITE]);
				free_command(p->rightcmd);
				free_command(p->leftcmd);
				free_command(cmd);
				free_alt_stack();
				_exit(EXECERROR);
			}

			if (right_pid == 0) {
				free_command(p->leftcmd);
				close(pipe_cmd[PIPE_WRITE]);
				dup2(pipe_cmd[PIPE_READ], STDIN_FILENO);
				close(pipe_cmd[PIPE_READ]);

				exec_cmd(p->rightcmd);
			} else {
				close(pipe_cmd[PIPE_READ]);
				close(pipe_cmd[PIPE_WRITE]);

				waitpid(left_pid, NULL, 0);
				waitpid(right_pid, NULL, 0);
			}
		}

		free_command(cmd);
		free_alt_stack();
		_exit(EXECSUCCESS);
	}
	}
}
