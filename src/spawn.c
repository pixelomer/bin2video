#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "spawn.h"

int spawn_process(char **argv, pid_t *new_pid, int *stdin_fd, int *stdout_fd) {
	int stdin_pipe[2];
	int stdout_pipe[2];
	if (stdin_fd != NULL) {
		pipe((int *)&stdin_pipe);
	}
	if (stdout_fd != NULL) {
		pipe((int *)&stdout_pipe);
	}

	int ret = fork();
	if (ret == 0) {
		if (stdin_fd != NULL) {
			if (stdin_pipe[0] != STDIN_FILENO) {
				dup2(stdin_pipe[0], STDIN_FILENO);
				close(stdin_pipe[0]);
			}
			close(stdin_pipe[1]);
		}
		if (stdout_fd != NULL) {
			if (stdout_pipe[1] != STDOUT_FILENO) {
				dup2(stdout_pipe[1], STDOUT_FILENO);
				close(stdout_pipe[1]);
			}
			close(stdout_pipe[0]);
		}
		ret = execvp(*argv, argv);
		perror("couldn't spawn process");
		exit(1);
	}
	if (ret == -1) {
		return -1;
	}
	
	*new_pid = ret;
	if (stdin_fd != NULL) {
		*stdin_fd = stdin_pipe[1];
	}
	if (stdout_fd != NULL) {
		*stdout_fd = stdout_pipe[0];
	}
	return 0;
}