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
		"  %s [options] -p pngs-concat.bin data.bin\n"
		"\n"
		"OPTIONS:\n"
		"  -b <bits> Bits per pixel. Supported values: 1, 6, 8, 24\n"
		"            Default value is 1 (black and white). Higher values\n"
		"            are more likely to be lost after compression. 24 bit\n"
		"            mode will almost certainly result in corrupted files,\n"
		"            so don't use it.\n"
		"            Only useful in encode mode.\n"
		"  -e        Encode mode. Takes an input binary file and produces\n"
		"            a video file.\n"
		"  -d        Decode mode. Takes an input video file and produces\n"
		"            the original file.\n"
		"  -p        Decode mode for concatenated PNG files. Useful for\n"
		"            testing.\n";
#define USAGE() { \
	fprintf(stderr, usage_str, argv[0], argv[0], argv[0]); \
	return EXIT_FAILURE; \
}
	char *input_file = NULL;
	char *output_file = NULL;
	char operation_mode = 0;
	enum b2v_pixel_mode pixel_mode = 1;
	int opt;
	bool opts[0x100] = {};
	while ((opt = getopt(argc, argv, "b:dep")) != -1) {
		if (opts[opt & 0xFF]) USAGE();
		opts[opt & 0xFF] = true;
		switch (opt) {
			case 'b':
				errno = 0;
				pixel_mode = strtol(optarg, NULL, 10);
				if (errno != 0) USAGE();
				break;
			case 'd':
			case 'e':
			case 'p':
				if (operation_mode != 0) USAGE();
				operation_mode = opt;
				break;
			default:
				USAGE();
		}
	}
	if (optind != (argc - 2)) {
		USAGE();
	}
	input_file = argv[optind];
	output_file = argv[optind + 1];
	switch (operation_mode) {
		case 'd':
			return b2v_decode(input_file, output_file);
		case 'p':
			b2v_decode_png(fopen(input_file, "r"), fopen(output_file, "w"));
			return EXIT_SUCCESS;
		case 'e':
			switch (pixel_mode) {
				case B2V_1BIT_PER_PIXEL:
				case B2V_6BIT_PER_PIXEL:
				case B2V_8BIT_PER_PIXEL:
				case B2V_24BIT_PER_PIXEL:
					break;
				default:
					USAGE();
			}
			return b2v_encode(input_file, output_file, 1, pixel_mode);
		default:
			USAGE();
	}
}