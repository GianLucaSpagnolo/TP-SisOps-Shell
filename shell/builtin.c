#include "builtin.h"
#include "utils.h"

#define BUILTIN_ERROR 0
#define BUILTIN_SUCCESS 1

static int
starts_with(char *str, char *condition)
{
	int i;
	for (i = 0; condition[i] != END_STRING; i++) {
		if (str[i] != condition[i]) {
			return 0;
		}
	}
	return 1;
}

// returns true if the 'exit' call
// should be performed
//
// (It must not be called from here)
int
exit_shell(char *cmd)
{
	if (starts_with(cmd, "exit") == 1) {
		return BUILTIN_SUCCESS;
	}
	return BUILTIN_ERROR;
}

static int
cd_path(char *cmd)
{
	int status;
	char *path = &cmd[3];
	status = chdir(path);
	if (status == 0) {
		if (starts_with(path, "..") == 1) {
			for (int i = strlen(prompt) - 1; i >= 0; i--) {
				if (prompt[i] == '/') {
					prompt[i] = ')';
					prompt[i + 1] = END_STRING;
					break;
				}
			}
		} else {
			strcpy(&prompt[strlen(prompt) - 1], "/");
			strcpy(&prompt[strlen(prompt)], path);
			prompt[strlen(prompt)] = ')';
		}
		return BUILTIN_SUCCESS;
	}
	return BUILTIN_ERROR;
}

static int
cd_no_path()
{
	char *home = getenv("HOME");
	int status = chdir(home);
	if (status == 0) {
		prompt[0] = '(';
		strcpy(&prompt[1], home);
		prompt[strlen(home) + 1] = ')';
		prompt[strlen(home) + 2] = END_STRING;
		return BUILTIN_SUCCESS;
	}
	return BUILTIN_ERROR;
}

// returns true if "chdir" was performed
//  this means that if 'cmd' contains:
// 	1. $ cd directory (change to 'directory')
// 	2. $ cd (change to $HOME)
//  it has to be executed and then return true
//
//  Remember to update the 'prompt' with the
//  	new directory.
//
// Examples:
//  1. cmd = ['c','d', ' ', '/', 'b', 'i', 'n', '\0']
//  2. cmd = ['c','d', '\0']
int
cd(char *cmd)
{
	if (strlen(cmd) > 3 && starts_with(cmd, "cd ") == 1) {
		return cd_path(cmd);
	} else if (strlen(cmd) == 2 && starts_with(cmd, "cd") == 1) {
		return cd_no_path();
	}

	if (strlen(cmd) > 2 && starts_with(cmd, "cd") == 1) {
		return BUILTIN_SUCCESS;
	}
	return BUILTIN_ERROR;
}

// returns true if 'pwd' was invoked
// in the command line
//
// (It has to be executed here and then
// 	return true)
int
pwd(char *cmd)
{
	if (starts_with(cmd, "pwd") == 1) {
		char buf[BUFLEN] = { 0 };
		char *pwd = getcwd(buf, sizeof(buf));
		if (pwd != NULL) {
			printf_debug("%s\n", pwd);
			return BUILTIN_SUCCESS;
		}
	}
	return BUILTIN_ERROR;
}

// returns true if `history` was invoked
// in the command line
//
// (It has to be executed here and then
// 	return true)
int
history(char *cmd)
{
	// Your code here

	return 0;
}
