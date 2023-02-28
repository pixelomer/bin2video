#ifndef B2V_BIN2VIDEO_H
#define B2V_BIN2VIDEO_H

int b2v_encode(const char *input, const char *output, int block_size,
	int bits_per_pixel);
int b2v_decode(const char *input, const char *output,
	int bits_per_pixel);

#endif