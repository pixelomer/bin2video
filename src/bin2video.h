#ifndef B2V_BIN2VIDEO_H
#define B2V_BIN2VIDEO_H

#include <stdbool.h>

int b2v_encode(const char *input, const char *output, int real_width,
	int real_height, int initial_block_size, int block_size, int bits_per_pixel,
	int framerate, const char **encode_argv, bool isg_mode);
int b2v_decode(const char *input, const char *output, int initial_block_size,
	bool isg_mode);

#endif