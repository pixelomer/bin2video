#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include "stbi_image_write_fd.h"
#include "bin2video.h"
#include "spawn.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stb_image_write.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

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

int read_next_bits(FILE *file, int bits, int *byte, int *bit) {
	int ret=0, i;
	for (i=0; i<bits; i++) {
		ret <<= 1;
		if (feof(file)) {
			continue;
		}
		int new_bit = read_next_bit(file, byte, bit);
		if (new_bit != -1) {
			ret |= new_bit;
		}
	}
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

void write_next_bits(FILE *file, int value, int bit_size, int *byte, int *bit) {
	for (int i=0; i<bit_size; i++) {
		write_next_bit(file, value & 1, byte, bit);
	}
}

void b2v_decode_png(FILE *image_file, FILE *output_file) {
	enum b2v_pixel_mode pixel_mode = B2V_6BIT_PER_PIXEL;

	int bit=0, byte=0;
	while (!feof(image_file)) {
		int width, height, comp;
		stbi_uc *image_data = stbi_load_from_file(image_file, &width, &height,
			&comp, 3);
		if (image_data == NULL) {
			continue;
		}
		printf("w=%d, h=%d, comp=%d, data=%p\n", width, height, comp, image_data);
		if (comp < 3) {
			// ???
			stbi_image_free(image_data);
			continue;
		}
		int pixels = width * height;
		for (int i=0; i<pixels; i++) {
			#define PIXEL(j, bits) (int)round((double)image_data[i * comp + j] / \
				(double)(~(uint8_t)0 >> (8 - bits))) >> bits
			switch (pixel_mode) {
				int value;
				case B2V_1BIT_PER_PIXEL:
					value = ((int)image_data[i * comp] + (int)image_data[i * comp + 1]
						+ (int)image_data[i * comp + 2]) / 3;
					value = (value > 127) ? 1 : 0;
					write_next_bit(output_file, value, &byte, &bit);
					break;
				case B2V_6BIT_PER_PIXEL:
					for (int j=0; j<3; j++) {
						value = PIXEL(j, 2);
						write_next_bits(output_file, value, 2, &byte, &bit);
					}
					break;
				case B2V_8BIT_PER_PIXEL:
					value = (PIXEL(0, 2) << 6) | (PIXEL(1, 3) << 3) | PIXEL(2, 3);
					fputc(value, output_file);
					break;
				case B2V_24BIT_PER_PIXEL:
					for (int j=0; j<3; j++) {
						fputc(image_data[i * comp + j], output_file);
					}
					break;
			}
			#undef PIXEL
		}
		stbi_image_free(image_data);
	}
}

int b2v_decode(const char *input, const char *output) {
	FILE *output_file = fopen(output, "w");
	if (output_file == NULL) {
		perror("couldn't open output for writing");
		return EXIT_FAILURE;
	}
	
	int ffmpeg_stdout = -1;
	pid_t ffmpeg_pid = -1;
	char *argv[] = { "ffmpeg", "-i", (char *)input, "-c:v", "png", "-f",
		"image2pipe", "-vf", "scale=320:180:flags=neighbor", "-", NULL };
	if ( spawn_process(argv, &ffmpeg_pid, NULL, &ffmpeg_stdout) == -1 ) {
		perror("couldn't spawn ffmpeg");
	}

	FILE *image_file = fdopen(ffmpeg_stdout, "r");
	
	b2v_decode_png(image_file, output_file);

	fclose(image_file);
	fclose(output_file);

	return EXIT_SUCCESS;
}

int b2v_encode(const char *input, const char *output, int block_size,
	enum b2v_pixel_mode pixel_mode)
{
	FILE *input_file = fopen(input, "r");
	if (input_file == NULL) {
		perror("couldn't open input for reading");
		return EXIT_FAILURE;
	}

	int input_fd = ftell(input_file);
	struct stat input_stat; // .st_size is size in bytes
	if (fstat(input_fd, &input_stat) != 0) {
		perror("couldn't stat() input file");
		return EXIT_FAILURE;
	}

	int ffmpeg_stdin = -1;
	pid_t ffmpeg_pid = -1;
	char *argv[] = { "ffmpeg", "-f", "image2pipe", "-framerate", "30", "-i",
		"-", "-c:v", "libx264", "-vf", "format=yuv420p,scale=1280:720:flags=neighbor",
		"-movflags", "+faststart", (char *)output, "-hide_banner",
		"-y", NULL };
	if ( spawn_process(argv, &ffmpeg_pid, &ffmpeg_stdin, NULL) == -1 ) {
		perror("couldn't spawn ffmpeg");
		return EXIT_FAILURE;
	}

	const int width = 320;
	const int height = 180;
	const int pixels = width * height;
	
	uint8_t *image_data = malloc(pixels * 3);

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
			case B2V_6BIT_PER_PIXEL:
				for (int i=0; i<3; i++) {
					image_data[pixel_idx * 3 + i] = read_next_bits(input_file, 2, &tbyte,
						&tbit) << 6;
				}
				break;
			case B2V_8BIT_PER_PIXEL:
				value = fgetc(input_file) & 0xFF;
				image_data[pixel_idx * 3 + 0] = (value & 0b00000011) << 6;
				image_data[pixel_idx * 3 + 1] = (value & 0b00011100) << 3;
				image_data[pixel_idx * 3 + 2] = (value & 0b11100000);
				break;
			case B2V_24BIT_PER_PIXEL:
				for (int i=0; i<3; i++) {
					image_data[pixel_idx * 3 + i] = fgetc(input_file) & 0xFF;
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
			int ret = stbi_write_png_to_fd(ffmpeg_stdin, width, height, 3, image_data, 0);
			if (ret < 0) {
				return EXIT_FAILURE;
			}
			pixel_idx = 0;
		}
	}

	close(ffmpeg_stdin);
	int ffmpeg_status;
	waitpid(ffmpeg_pid, &ffmpeg_status, 0);

	return WEXITSTATUS(ffmpeg_status);
}