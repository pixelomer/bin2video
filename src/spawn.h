#ifndef B2V_SPAWN_H
#define B2V_SPAWN_H

#include <unistd.h>

int spawn_process(char **argv, pid_t *new_pid, int *stdin_fd, int *stdout_fd);

#endif