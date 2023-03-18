#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include "bin2video.h"

#define MINIMUM_BLOCK_COUNT 200
#define STR(x) #x
#define STR_VAL(x) STR(x)

#define DEFAULT_WIDTH 1280
#define DEFAULT_HEIGHT 720
#define DEFAULT_BITS 1
#define DEFAULT_ISG_INITIAL_BLOCK_SIZE 5
#define DEFAULT_INITIAL_BLOCK_SIZE 10
#define DEFAULT_FRAMERATE 10
#define DEFAULT_FRAME_WRITE 1
#define DEFAULT_DATA_HEIGHT -1
#define DEFAULT_BLOCK_SIZE 5
//FIXME: Changing DEFAULT_FFMPEG does not actually change the default arguments
#define DEFAULT_FFMPEG "-c:v libx264 -pix_fmt yuv420p"

#if DEFAULT_BITS == 1
#define DEFAULT_BITS_DESC " (black and white)"
#else
#define DEFAULT_BITS_DESC ""
#endif

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
		"  -f <rate>   Framerate. Defaults to %d. Set to -1 to let FFmpeg\n"
		"              decide.\n"
		"  -c <n>      Write every frame n times. Defaults to %d. Cannot be\n"
		"              used with -I.\n"
		"  -b <bits>   Bits per pixel. Defaults to %d" DEFAULT_BITS_DESC ".\n"
		"  -w <width>  Video width. Defaults to %d.\n"
		"  -h <height> Video height. Defaults to %d.\n"
		"  -H <height> Data height. Set this to a value less than the video\n"
		"              height to limit the data blocks to a region on top of\n"
		"              the video. The bottom of the region will be black.\n"
		"              A value of -1 disables the data height. Defaults to %d.\n"
		"              Cannot be used with -I.\n"
		"  -s <size>   Size of each block. Defaults to %d.\n"
		"  -I          Infinite-Storage-Glitch compatibility mode.\n"
		"  -E          End the output with a black frame. Cannot be used with\n"
		"              -I.\n"
		"\n"
		"ADVANCED OPTIONS:\n"
		"  -S <size>   Sets the size of each block for the initial frame.\n"
		"              Defaults to %d. Do not change this unless you have\n"
		"              a good reason to do so. If you specify this flag\n"
		"              while encoding, you will also need to do it while\n"
		"              decoding. When -I is used, this value defaults to %d\n"
		"              and cannot be changed.\n"
		"  --          Options following -- will be treated as arguments for\n"
		"              FFmpeg. Defaults to \"%s\".\n"
		"              Has no effect in decode mode.\n"
		, argv0, argv0, DEFAULT_FRAMERATE, DEFAULT_FRAME_WRITE, DEFAULT_BITS,
		DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_DATA_HEIGHT, DEFAULT_BLOCK_SIZE,
		DEFAULT_INITIAL_BLOCK_SIZE, DEFAULT_ISG_INITIAL_BLOCK_SIZE,
		DEFAULT_FFMPEG);
}

#define USAGE() { usage(argv[0]); return EXIT_FAILURE; }
#define DIE(message) { fprintf(stderr, "%s: %s\n", argv[0], message); \
	return EXIT_FAILURE; }
#define NUM_ARG(target, min) { \
	errno = 0; \
	target = strtol(optarg, NULL, 10); \
	if ((errno != 0) || (target < min)) USAGE(); \
}
int main(int argc, char **argv) {
	char *input_file = NULL;
	char *output_file = NULL;
	char operation_mode = 0;

	int bits_per_pixel = DEFAULT_BITS;
	int initial_block_size = DEFAULT_INITIAL_BLOCK_SIZE;
	int block_size = DEFAULT_BLOCK_SIZE;
	int data_height = DEFAULT_DATA_HEIGHT;
	int frame_write = DEFAULT_FRAME_WRITE;
	bool black_frame = false;
	int width = DEFAULT_WIDTH;
	int height = DEFAULT_HEIGHT;
	bool write_to_tty = false;
	int framerate = DEFAULT_FRAMERATE;
	bool isg_mode = false;

	int opt;
	bool opts[0x100] = {};
	while ((opt = getopt(argc, argv, "f:b:w:h:s:S:i:o:detIH:c:E")) != -1) {
		if (opts[opt & 0xFF]) USAGE();
		opts[opt & 0xFF] = true;
		switch (opt) {
			case 'b': NUM_ARG(bits_per_pixel, 1); break;
			case 'w': NUM_ARG(width, 1); break;
			case 'h': NUM_ARG(height, 1); break;
			case 'S':
				NUM_ARG(initial_block_size, 1);
				opts['I'] = true;
				break;
			case 's': NUM_ARG(block_size, 1); break;
			case 'c':
				NUM_ARG(frame_write, 1);
				opts['I'] = true;
				break;
			case 'f': NUM_ARG(framerate, -1); break;
			case 'H':
				NUM_ARG(data_height, -1);
				opts['I'] = true;
				break;
			case 'i': input_file = optarg; break;
			case 'I':
				isg_mode = true;
				opts['H'] = true;
				opts['c'] = true;
				opts['S'] = true;
				break;
			case 'E': black_frame = true; break;
			case 'o': output_file = optarg; break;
			case 't': write_to_tty = true; break;
			case 'd':
			case 'e':
				if (operation_mode != 0) USAGE();
				operation_mode = opt;
				break;
			case '?':
			default:
				USAGE();
		}
	}

	const char *default_encode_argv[] = { "-c:v", "libx264", "-pix_fmt", "yuv420p",
		NULL };
	const char **encode_argv = default_encode_argv;
	if (optind != argc) {
		// argv[argc] is always NULL
		encode_argv = (const char **)argv + optind;
	}

	if (operation_mode == 0) {
		USAGE();
	}
	if ((bits_per_pixel < 0) || (bits_per_pixel > 24)) {
		DIE("bits-per-pixel must be in the range [0..24]")
	}
	if ((framerate != -1) && (framerate <= 0)) {
		DIE("framerate must be either -1 or a value greater than 0");
	}
	if ((width % initial_block_size != 0) || (height % initial_block_size != 0) ||
		(width % block_size != 0) || (height % block_size != 0))
	{
		DIE("width and height must be divisible to the initial and real block size");
	}
	if ((width % 2 != 0) || (height % 2 != 0)) {
		DIE("width and height must be divisible by 0");
	}
	if (data_height <= 0) {
		data_height = height;
	}
	else if (data_height >= height) {
		fprintf(stderr, "warning: data height is greater than or equal to the "
			"video height, it will have no effect\n");
		data_height = height;
	}
	else if ((data_height % block_size != 0) || (data_height % initial_block_size != 0)) {
		DIE("data height must be divisible by the initial and the real block size");
	}
	if (((width * data_height) / (initial_block_size * initial_block_size)) < MINIMUM_BLOCK_COUNT ||
		((width * data_height) / (block_size * block_size)) < MINIMUM_BLOCK_COUNT)
	{
		DIE("a minimum of " STR_VAL(MINIMUM_BLOCK_COUNT) " blocks must be available "
			"at all times, make sure the width and data height are big enough");
	}
	if (isg_mode) {
		initial_block_size = DEFAULT_ISG_INITIAL_BLOCK_SIZE;
		if ((bits_per_pixel != 1) && (bits_per_pixel != 24)) {
			DIE("bits-per-pixel must be either 1 or 24 in Infinite-Storage-Glitch mode");
		}
	}
	int ret;
	switch (operation_mode) {
		case 'd':
			if (input_file == NULL) {
				DIE("input file cannot be stdin in decode mode");
			}
			if ((output_file == NULL) && isatty(STDOUT_FILENO) && !write_to_tty) {
				DIE("refusing to write binary data to tty");
			}
			ret = b2v_decode(input_file, output_file, initial_block_size, isg_mode);
			break;
		case 'e': {
			if (output_file == NULL) {
				DIE("output file cannot be stdout in encode mode");
			}
			ret = b2v_encode(input_file, output_file, width, height,
				initial_block_size, block_size, bits_per_pixel, framerate,
				encode_argv, isg_mode, data_height, frame_write, black_frame);
			break;
		}
	}
	return ret;
}
#undef USAGE