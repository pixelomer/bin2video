#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include "bin2video.h"
#include "subprocess.h"

#define INTERNAL_WIDTH 320
#define INTERNAL_HEIGHT 180
#define VIDEO_SCALE 4
#define VIDEO_WIDTH (VIDEO_SCALE * INTERNAL_WIDTH)
#define VIDEO_HEIGHT (VIDEO_SCALE * INTERNAL_HEIGHT)

//FIXME: not affected by other definitions
#define VIDEO_RESOLUTION "1280x720"

// scale_up(image_in, image_out, 100, 100, 5)
//   --> takes a 100x100 image, returns a 500x500 image
// image_in must be (height * width * 3) bytes
// image_out must be (height * scale * width * scale * 3) bytes
void scale_up(uint8_t *in, uint8_t *out, int in_width, int in_height, int scale) {
	for (int y=0; y<in_height; y++) {
		uint8_t *scaled_line = &out[in_width * scale * 3 * y * scale];
		uint8_t *scaled_line_pt = scaled_line;
		for (int x=0; x<in_width; x++) {
			uint8_t *source_pixel = &in[(y * in_width + x) * 3];
			for (int i=0; i<scale; i++) {
				memcpy(scaled_line_pt, source_pixel, 3);
				scaled_line_pt += 3;
			}
		}
		int diff = scaled_line_pt - scaled_line;
		for (int i=1; i<scale; i++) {
			memcpy(scaled_line + diff * i, scaled_line, diff);
		}
	}
}

// scale_down(image_in, image_out, 100, 100, 5)
//   --> takes a 500x500 image, returns a 100x100 image
// image_in must be (height * scale * width * scale * 3) bytes
// image_out must be (height * width * 3) bytes
void scale_down(uint8_t *in, uint8_t *out, int out_width, int out_height, int scale) {
	int in_width = out_width * scale;
	int in_height = out_height * scale;
	for (int y=0; y<out_height; y++) {
		for (int x=0; x<out_width; x++) {
			for (int i=0; i<3; i++) {
				uint32_t sum = 0;
				for (int sy = y * scale; sy < (y + 1) * scale; sy++) {
					for (int sx = x * scale; sx < (x + 1) * scale; sx++) {
						sum += in[(sy * in_width + sx) * 3 + i];
					}
				}
				out[(y * out_width + x) * 3 + i] = (uint8_t)(sum / (scale * scale));
			}
		}
	}
}

int read_next_bit(FILE *file, int *byte, int *bit) {
	if (*bit == 0) {
		int c = fgetc(file);
		if (c == EOF) {
			return EOF;
		}
		*byte = c;
	}
	int ret = ((*byte) >> (*bit)) & 1;
	*bit = (1 + *bit) % 8;
	return ret;
}

void write_next_bit(FILE *file, int value, int *byte, int *bit) {
	*byte |= value << *bit;
	*bit += 1;
	if (*bit == 8) {
		fputc(*byte, file);
		*bit = 0;
		*byte = 0;
	}
}

int b2v_decode(const char *input, const char *output,
	enum b2v_pixel_mode pixel_mode)
{
	FILE *output_file = fopen(output, "wb");
	if (output_file == NULL) {
		perror("couldn't open output for writing");
		return EXIT_FAILURE;
	}
	
	char *argv[] = { "ffmpeg", "-i", (char *)input, "-f", "rawvideo", "-pix_fmt",
		"rgb24", "-", "-v", "quiet", "-hide_banner", NULL };
	struct subprocess_s ffmpeg_process;
	int subprocess_ret = subprocess_create((const char * const *)argv,
		subprocess_option_no_window | subprocess_option_inherit_environment |
		subprocess_option_search_user_path, &ffmpeg_process);
	if ( subprocess_ret != 0 ) {
		perror("couldn't spawn ffmpeg");
		fclose(output_file);
		return EXIT_FAILURE;
	}

	FILE *ffmpeg_stdout = ffmpeg_process.stdout_file;

	int bit=0, byte=0;

	const int scale = VIDEO_SCALE;
	const int width = INTERNAL_WIDTH;
	const int height = INTERNAL_HEIGHT;
	const int pixels = width * height;
	
	uint8_t *image_data = malloc(pixels * 3);
	uint8_t *image_scaled = malloc(pixels * scale * scale * 3);

	int result = -1;
	while (result == -1) {
		int read_ret = fread(image_scaled, pixels * scale * scale * 3, 1,
			ffmpeg_stdout);
		if (read_ret == -1) {
			result = EXIT_FAILURE;
			break;
		}
		else if (read_ret == 0) {
			result = EXIT_SUCCESS;
			break;
		}
		scale_down(image_scaled, image_data, width, height, scale);
		for (int i=0; i<pixels; i++) {
			switch (pixel_mode) {
				int value;
				case B2V_1BIT_PER_PIXEL:
					value = ((int)image_data[i * 3] + (int)image_data[i * 3 + 1]
						+ (int)image_data[i * 3 + 2]) / 3;
					value = (value > 127) ? 1 : 0;
					write_next_bit(output_file, value, &byte, &bit);
					break;
				case B2V_3BIT_PER_PIXEL:
					for (int j=0; j<3; j++) {
						value = image_data[i * 3 + j];
						value = (value > 127) ? 1 : 0;
						write_next_bit(output_file, value, &byte, &bit);
					}
					break;
			}
		}
	}

	fclose(ffmpeg_stdout);
	free(image_data);
	fclose(output_file);
	
	subprocess_join(&ffmpeg_process, NULL);
	subprocess_destroy(&ffmpeg_process);
	if (result == 0) {
		return ffmpeg_process.return_status;
	}
	else {
		return result;
	}
}

int b2v_encode(const char *input, const char *output, int block_size,
	enum b2v_pixel_mode pixel_mode)
{
	FILE *input_file = fopen(input, "rb");
	if (input_file == NULL) {
		perror("couldn't open input for reading");
		return EXIT_FAILURE;
	}

	int input_fd = ftell(input_file);
	struct stat input_stat; // .st_size is size in bytes
	if (fstat(input_fd, &input_stat) != 0) {
		perror("couldn't stat() input file");
		fclose(input_file);
		return EXIT_FAILURE;
	}

	struct subprocess_s ffmpeg_process;
	char *argv[] = { "ffmpeg", "-framerate", "30", "-s", VIDEO_RESOLUTION, "-f",
		"rawvideo", "-pix_fmt", "rgb24", "-i", "-", "-c:v",
		"libx264", "-pix_fmt", "yuv420p", "-movflags", "+faststart", (char *)output,
		"-hide_banner", "-y", "-v", "quiet", NULL };
	int subprocess_ret = subprocess_create((const char * const *)argv,
		subprocess_option_no_window | subprocess_option_inherit_environment |
		subprocess_option_search_user_path, &ffmpeg_process);
	if ( subprocess_ret == -1 ) {
		fprintf(stderr, "couldn't spawn ffmpeg\n");
		fclose(input_file);
		return EXIT_FAILURE;
	}

	FILE *ffmpeg_stdin = ffmpeg_process.stdin_file;
	FILE *ffmpeg_stderr = ffmpeg_process.stderr_file;

	const int scale = VIDEO_SCALE;
	const int width = INTERNAL_WIDTH;
	const int height = INTERNAL_HEIGHT;
	const int pixels = width * height;
	
	uint8_t *image_data = malloc(pixels * 3);
	uint8_t *image_scaled = malloc(pixels * scale * scale * 3);

	int tbit=0, tbyte=0;
	int pixel_idx = 0;
	while ( !feof(input_file) ) {
		switch (pixel_mode) {
			int value;
			case B2V_1BIT_PER_PIXEL:
				value = read_next_bit(input_file, &tbyte, &tbit);
				value = value ? 0xFF : 0x00;
				memset(image_data + (pixel_idx * 3), value, 3);
				break;
			case B2V_3BIT_PER_PIXEL:
				for (int i=0; i<3; i++) {
					value = read_next_bit(input_file, &tbyte, &tbit);
					value = value ? 0xFF : 0x00;
					image_data[pixel_idx * 3 + i] = value;
				}
				break;
		}
		pixel_idx += 1;
		if (feof(input_file)) {
			uint8_t *start = image_data + (pixel_idx * 3);
			uint8_t *end = image_data + (pixels * 3);
			memset(start, 0, end - start);
			pixel_idx = pixels;
		}
		if (pixel_idx >= pixels) {
			scale_up(image_data, image_scaled, width, height, scale);
			fwrite(image_scaled, width * height * scale * scale * 3, 1, ffmpeg_stdin);
			pixel_idx = 0;
		}
	}

	fclose(input_file);
	fclose(ffmpeg_stdin);
	free(image_data);

	subprocess_ret = subprocess_join(&ffmpeg_process, NULL);
	subprocess_destroy(&ffmpeg_process);
	if (subprocess_ret == 0) {
		return ffmpeg_process.return_status;
	}
	else {
		return subprocess_ret;
	}
}