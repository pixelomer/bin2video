#ifndef B2V_BIN2VIDEO_H
#define B2V_BIN2VIDEO_H

enum b2v_pixel_mode {
	B2V_1BIT_PER_PIXEL = 1,
	B2V_6BIT_PER_PIXEL = 6,
	B2V_8BIT_PER_PIXEL = 8,
	B2V_24BIT_PER_PIXEL = 24
};

int b2v_encode(const char *input, const char *output, int block_size,
	enum b2v_pixel_mode pixel_mode);
int b2v_decode(const char *input, const char *output);
void b2v_decode_png(FILE *image_file, FILE *output_file);

#endif