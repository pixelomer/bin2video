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
		"  -b <bits> Bits per pixel. Supported values: 1, 3\n"
		"            Default value is 1 (black and white). Higher values\n"
		"            are more likely to cause corruption.\n"
		"  -e        Encode mode. Takes an input binary file and produces\n"
		"            a video file.\n"
		"  -d        Decode mode. Takes an input video file and produces\n"
		"            the original file.\n";
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
			return b2v_encode(input_file, output_file, 1, bits_per_pixel);
		default:
			USAGE();
	}
}