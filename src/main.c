#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include "bin2video.h"

#define DEFAULT_WIDTH 1280
#define DEFAULT_HEIGHT 720
#define DEFAULT_BITS 1
#define DEFAULT_INITIAL_BLOCK_SIZE 4
#define DEFAULT_BLOCK_SIZE 4

void usage(char *argv0) {
	fprintf(stderr,
		"USAGE:\n"
		"  %s [options] -e data.bin video.mp4\n"
		"  %s [options] -d video.mp4 data.bin\n"
		"\n"
		"OPTIONS:\n"
		"  -e          Encode mode. Takes an input binary file and produces\n"
		"              a video file.\n"
		"  -d          Decode mode. Takes an input video file and produces\n"
		"              the original binary file.\n"
		"  -i          Input file. Defaults to stdin.\n"
		"  -o          Output file. Defaults to stdout.\n"
		"  -t          Allows writing output to a tty.\n"
		"  -b <bits>   Bits per pixel. Defaults to %d (black and white).\n"
		"  -w <width>  Sets video width. Defaults to %d.\n"
		"  -h <height> Sets video height. Defaults to %d.\n"
		"  -s <size>   Sets the size of each block. Defaults to %d.\n"
		"\n"
		"ADVANCED OPTIONS:\n"
		"  -S <size>   Sets the size of each block for the initial frame.\n"
		"              Defaults to %d. Do not change this unless you have\n"
		"              a good reason to do so. If you specify this flag\n"
		"              while encoding, you will also need to do it while\n"
		"              decoding.\n"
		"\n"
		"NOTES:\n"
		"  Increasing the number of bits per pixel will increase the risk of\n"
		"  corruption. Increasing the block size will decrease the risk of\n"
		"  corruption.\n"
		, argv0, argv0, DEFAULT_BITS, DEFAULT_WIDTH, DEFAULT_HEIGHT,
		DEFAULT_BLOCK_SIZE, DEFAULT_INITIAL_BLOCK_SIZE);
}

#define USAGE() { usage(argv[0]); return EXIT_FAILURE; }
#define DIE(message) { fprintf(stderr, "%s: %s\n", argv[0], message); \
	return EXIT_FAILURE; }
#define NUM_ARG(target) { \
	errno = 0; \
	target = strtol(optarg, NULL, 10); \
	if ((errno != 0) || (target < 0)) USAGE(); \
}
int main(int argc, char **argv) {
	char *input_file = NULL;
	char *output_file = NULL;
	char operation_mode = 0;

	int bits_per_pixel = DEFAULT_BITS;
	int initial_block_size = DEFAULT_INITIAL_BLOCK_SIZE;
	int block_size = DEFAULT_BLOCK_SIZE;
	int width = DEFAULT_WIDTH;
	int height = DEFAULT_HEIGHT;
	bool write_to_tty = false;

	int opt;
	bool opts[0x100] = {};
	while ((opt = getopt(argc, argv, "b:w:h:s:S:i:o:de")) != -1) {
		if (opts[opt & 0xFF]) USAGE();
		opts[opt & 0xFF] = true;
		switch (opt) {
			case 'b': NUM_ARG(bits_per_pixel); break;
			case 'w': NUM_ARG(width); break;
			case 'h': NUM_ARG(height); break;
			case 'S': NUM_ARG(initial_block_size); break;
			case 's': NUM_ARG(block_size); break;
			case 'i': input_file = optarg; break;
			case 'o': output_file = optarg; break;
			case 't': write_to_tty = true; break;
			case 'd':
			case 'e':
				if (operation_mode != 0) USAGE();
				operation_mode = opt;
				break;
			default:
				USAGE();
		}
	}
	if (optind != argc) {
		USAGE();
	}
	if (operation_mode == 0) {
		USAGE();
	}
	if ((bits_per_pixel < 0) || (bits_per_pixel > 24)) {
		DIE("bits-per-pixel must be in the range [0..24]")
	}
	if ((output_file == NULL) && isatty(STDIN_FILENO) && !write_to_tty) {
		DIE("refusing to write binary data to tty");
	}
	if ((width % initial_block_size != 0) || (height % initial_block_size != 0) ||
		(width % block_size != 0) || (height % block_size != 0))
	{
		DIE("width and height must be divisible to the initial and real block size");
	}
	if ((width % 2 != 0) || (height % 2 != 0)) {
		DIE("width and height must be divisible by 0");
	}
	if (width * height < 100) {
		DIE("(width * height) must be at least 100");
	}
	switch (operation_mode) {
		case 'd':
			if (input_file == NULL) {
				DIE("input file cannot be stdin in decode mode");
			}
			return b2v_decode(input_file, output_file, initial_block_size);
		case 'e':
			if (output_file == NULL) {
				DIE("output file cannot be stdout in encode mode");
			}
			return b2v_encode(input_file, output_file, width, height,
				initial_block_size, block_size, bits_per_pixel);
	}
}
#undef USAGE