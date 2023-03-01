#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include "bin2video.h"

int main(int argc, char **argv) {
	const char *usage_str = 
		"USAGE:\n"
		"  %s [options] -e data.bin video.mp4\n"
		"  %s [options] -d video.mp4 data.bin\n"
		"\n"
		"OPTIONS:\n"
		"  -e          Encode mode. Takes an input binary file and produces\n"
		"              a video file.\n"
		"  -d          Decode mode. Takes an input video file and produces\n"
		"              the original binary file.\n"
		"  -L          Shows license.\n"
		"  -b <bits>   Bits per pixel. Defaults to 1 (black and white).\n"
		"  -w <width>  Sets video width. Defaults to 1280.\n"
		"  -h <height> Sets video height. Defaults to 720.\n"
		"  -s <size>   Sets the size of each block. Defaults to 4.\n"
		"\n"
		"NOTES:\n"
		"  Increasing the number of bits per pixel will increase the risk of\n"
		"  corruption. Increasing the block size will decrease the risk of\n"
		"  corruption. With the default configuration, more than 3 bits per\n"
		"  pixel is very likely to cause corruption.\n";
#define USAGE() { \
	fprintf(stderr, usage_str, argv[0], argv[0]); \
	return EXIT_FAILURE; \
}
	char *input_file = NULL;
	char *output_file = NULL;
	char operation_mode = 0;
	int bits_per_pixel = 1;
	int opt;
	bool opts[0x100] = {};
	while ((opt = getopt(argc, argv, "b:de")) != -1) {
		if (opts[opt & 0xFF]) USAGE();
		opts[opt & 0xFF] = true;
		switch (opt) {
			case 'b':
				errno = 0;
				bits_per_pixel = strtol(optarg, NULL, 10);
				if (errno != 0) USAGE();
				break;
			case 'd':
			case 'e':
				if (operation_mode != 0) USAGE();
				operation_mode = opt;
				break;
			default:
				USAGE();
		}
	}
	if ((bits_per_pixel < 0) || (bits_per_pixel > 24)) {
		USAGE();
	}
	if (optind != (argc - 2)) {
		USAGE();
	}
	input_file = argv[optind];
	output_file = argv[optind + 1];
	switch (operation_mode) {
		case 'd':
			return b2v_decode(input_file, output_file, bits_per_pixel);
		case 'e':
			return b2v_encode(input_file, output_file, 1280, 720, 4, 4, bits_per_pixel);
		default:
			USAGE();
	}
}