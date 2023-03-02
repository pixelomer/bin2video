#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "bin2video.h"
#include "subprocess.h"

#define METADATA_VERSION 1

#define LOAD_UINT32(u8_pt) \
	(uint32_t)( \
		((u8_pt)[0] << 24) | \
		(((u8_pt)[1] << 16) & 0xFF0000) | \
		(((u8_pt)[2] << 8) & 0xFF00) | \
		((u8_pt)[3] & 0xFF) \
	)
#define STORE_UINT32(u8_pt, u32) { \
	(u8_pt)[0] = u32 >> 24; \
	(u8_pt)[1] = (u32 >> 16) & 0xFF; \
	(u8_pt)[2] = (u32 >> 8) & 0xFF; \
	(u8_pt)[3] = u32 & 0xFF; \
}

// array[bits_per_pixel][comp]

static bool did_init_before = false;
static int bits_per_comp[25][3];
static double comp_div[25][3];

int get_bit(uint8_t *buffer, int size, int *tbyte, int *tbit, int *idx)
{
	if (*tbit == 0) {
		if (*idx == size) {
			*tbyte = -1;
			return 0;
		}
		*tbyte = buffer[(*idx)++];
		*tbit = 0;
	}
	int ret = (*tbyte >> (*tbit)++) & 1;
	if (*tbit == 8) {
		*tbit = 0;
	}
	return ret;
}

void put_bit(uint8_t *buffer, int bit, int *tbyte, int *tbit, int *idx) {
	*tbyte |= bit << (*tbit)++;
	if (*tbit == 8) {
		buffer[(*idx)++] = *tbyte;
		*tbyte = 0;
		*tbit = 0;
	}
}

struct b2v_context {
	uint8_t *image;
	uint8_t *buffer;
	uint8_t *image_scaled;
	int scale;
	int tbyte;
	int tbit;
	int width;
	int height;
	int bits_per_pixel;
	size_t buffer_size;
	size_t bytes_available;
};

void b2v_context_realloc(struct b2v_context *ctx) {
	int blocks = ctx->width * ctx->height;

	if (ctx->buffer != NULL) free(ctx->buffer);
	ctx->buffer_size = (blocks * ctx->bits_per_pixel) / 8 + 1;
	ctx->buffer = malloc(ctx->buffer_size);
	
	if (ctx->image != NULL) free(ctx->image);
	ctx->image = malloc(blocks * 3);

	if (ctx->image_scaled != NULL) free(ctx->image_scaled);
	ctx->image_scaled = malloc(blocks * ctx->scale * ctx->scale * 3);

	ctx->tbit = 0;
	ctx->tbyte = 0;
	ctx->bytes_available = 0;
}

void b2v_context_init(struct b2v_context *ctx, int width, int height,
	int bits_per_pixel, int scale)
{
	if (!did_init_before) {
		did_init_before = true;
		for (int bits_per_pixel=0; bits_per_pixel<=24; bits_per_pixel++) {
			for (int i=0; i<3; i++) {
				bits_per_comp[bits_per_pixel][i] = bits_per_pixel / 3;
				if ((bits_per_pixel % 3) > i) {
					bits_per_comp[bits_per_pixel][i] += 1;
				}
				comp_div[bits_per_pixel][i] = 255.0 /
					(double)((1 << bits_per_comp[bits_per_pixel][i]) - 1);
			}
		}
	}

	memset(ctx, 0, sizeof(*ctx));
	ctx->width = width;
	ctx->height = height;
	ctx->scale = scale;
	ctx->bits_per_pixel = bits_per_pixel;
	b2v_context_realloc(ctx);
}

void b2v_context_destroy(struct b2v_context *ctx) {
	free(ctx->buffer);
	free(ctx->image);
	free(ctx->image_scaled);
}

int _b2v_fill_image_next(struct b2v_context *ctx, int bits_per_pixel,
	int start, int end, uint8_t *buffer, int *tbit, int *tbyte, int *buffer_idx)
{
	if (ctx->tbyte == -1) {
		ctx->tbyte = 0;
	}
	int i;
	for (i=start; (i < end) && (ctx->tbyte != -1); i++) {
		int value;
		switch (bits_per_pixel) {
			case 1:
				value = get_bit(buffer, ctx->bytes_available, tbyte, tbit,
					buffer_idx) * 0xFF;
				memset(ctx->image + (i * 3), value, 3);
				break;
			default:
				for (int c=0; c<3; c++) {
					value = 0;
					for (int b=0; b<bits_per_comp[ctx->bits_per_pixel][c]; b++) {
						value <<= 1;
						value |= get_bit(buffer, ctx->bytes_available, tbyte,
							tbit, buffer_idx);
					}
					value = (uint8_t)round(comp_div[ctx->bits_per_pixel][c] * (double)value);
					ctx->image[i * 3 + c] = value;
				}
				break;
		}
	}
	return i;
}

int b2v_fill_image(struct b2v_context *ctx) {
	int buffer_idx = 0;
	int blocks = ctx->width * ctx->height;

	uint8_t metadata[4];
	const int metadata_end = sizeof(metadata) * 8;
	
	int image_idx = _b2v_fill_image_next(ctx, ctx->bits_per_pixel, metadata_end,
		blocks, ctx->buffer, &ctx->tbit, &ctx->tbyte, &buffer_idx);
	memset(ctx->image + image_idx * 3, 0, (blocks - image_idx) * 3);

	STORE_UINT32(metadata, image_idx);
	int ret = buffer_idx, tbyte = 0, tbit = 0;
	buffer_idx = 0;
	image_idx = _b2v_fill_image_next(ctx, 1, 0, metadata_end, metadata, &tbit, &tbyte,
		&buffer_idx);

	// Scale image up
	for (int y=0; y<ctx->height; y++) {
		uint8_t *scaled_line = &ctx->image_scaled[ctx->width * ctx->scale * 3 * y
			* ctx->scale];
		uint8_t *scaled_line_pt = scaled_line;
		for (int x=0; x<ctx->width; x++) {
			uint8_t *source_pixel = &ctx->image[(y * ctx->width + x) * 3];
			for (int i=0; i<ctx->scale; i++) {
				memcpy(scaled_line_pt, source_pixel, 3);
				scaled_line_pt += 3;
			}
		}
		int diff = scaled_line_pt - scaled_line;
		for (int i=1; i<ctx->scale; i++) {
			memcpy(scaled_line + diff * i, scaled_line, diff);
		}
	}

	return ret;
}

void b2v_fill_image_from_file(struct b2v_context *ctx, FILE *file) {
	ctx->bytes_available += fread(ctx->buffer + ctx->bytes_available, 1,
		ctx->buffer_size - ctx->bytes_available, file);
	int next_idx = b2v_fill_image(ctx);
	memmove(ctx->buffer, ctx->buffer + next_idx, ctx->bytes_available - next_idx);
	ctx->bytes_available -= next_idx;
}

void _b2v_decode_image_next(struct b2v_context *ctx, int bits_per_pixel,
	int start, int end, uint8_t *buffer, int *tbit, int *tbyte, int *buffer_idx)
{
	for (int i=start; i<end; i++) {
		switch (bits_per_pixel) {
			int value;
			case 1:
				value = ((int)ctx->image[i * 3] + (int)ctx->image[i * 3 + 1]
					+ (int)ctx->image[i * 3 + 2]) / 3;
				value = (value > 127) ? 1 : 0;
				put_bit(buffer, value, tbyte, tbit, buffer_idx);
				break;
			default:
				for (int j=0; j<3; j++) {
					double color = (double)ctx->image[i * 3 + j];
					value = (int)round(color / comp_div[bits_per_pixel][j]);
					for (int b=bits_per_comp[bits_per_pixel][j]-1; b>=0; b--) {
						put_bit(buffer, ((uint8_t)value >> b) & 1, tbyte, tbit,
							buffer_idx);
					}
				}
				break;
		}
	}
}

int b2v_decode_image(struct b2v_context *ctx) {
	// Scale image down
	int scaled_width = ctx->width * ctx->scale;
	int scaled_height = ctx->height * ctx->scale;
	for (int y=0; y<ctx->height; y++) {
		for (int x=0; x<ctx->width; x++) {
			for (int i=0; i<3; i++) {
				uint32_t sum = 0;
				for (int sy = y * ctx->scale; sy < (y + 1) * ctx->scale; sy++) {
					for (int sx = x * ctx->scale; sx < (x + 1) * ctx->scale; sx++) {
						sum += ctx->image_scaled[(sy * scaled_width + sx) * 3 + i];
					}
				}
				ctx->image[(y * ctx->width + x) * 3 + i] = (uint8_t)(sum /
					(ctx->scale * ctx->scale));
			}
		}
	}

	int tbit=0, tbyte=0, buffer_idx=0;
	uint8_t metadata[4];
	const int metadata_end = sizeof(metadata) * 8;
	_b2v_decode_image_next(ctx, 1, 0, metadata_end, metadata, &tbit, &tbyte,
		&buffer_idx);
	uint32_t block_count = LOAD_UINT32(metadata);
	
	buffer_idx = 0;
	int max_blocks = ctx->width * ctx->height;
	int blocks = block_count;
	if (blocks > max_blocks) {
		blocks = max_blocks;
	}
	_b2v_decode_image_next(ctx, ctx->bits_per_pixel, metadata_end, blocks,
		ctx->buffer, &ctx->tbit, &ctx->tbyte, &buffer_idx);

	return buffer_idx;
}

void write_get_bit(FILE *file, int value, int *tbyte, int *tbit) {
	*tbyte |= value << *tbit;
	*tbit += 1;
	if (*tbit == 8) {
		fputc(*tbyte, file);
		*tbit = 0;
		*tbyte = 0;
	}
}

int spawn(const char **argv, struct subprocess_s *proc, bool enable_async) {
	int options = subprocess_option_no_window | subprocess_option_inherit_environment |
		subprocess_option_search_user_path;
	if (enable_async) {
		options |= subprocess_option_enable_async;
	}
	return subprocess_create((const char * const *)argv, options, proc);
}

int video_resolution(const char *file, int *width_pt, int *height_pt) {
	const char *argv[] = { "ffprobe", "-v", "error", "-select_streams", "v:0",
		"-show_entries", "stream=width,height", "-of", "csv=s=x:p=0", "--",
		file, NULL };
	
	struct subprocess_s ffmpeg_process;
	int subprocess_ret = spawn(argv, &ffmpeg_process, false);
	if ( subprocess_ret != 0 ) {
		fprintf(stderr, "couldn't spawn ffprobe\n");
		return -1;
	}

	int exit_code;
	subprocess_ret = subprocess_join(&ffmpeg_process, &exit_code);

	char resolution[33];
	memset(resolution, 0, sizeof(resolution));
	subprocess_read_stdout(&ffmpeg_process, resolution, sizeof(resolution)-1);
	fread(resolution, 1, sizeof(resolution)-1, ffmpeg_process.stdout_file);

	subprocess_destroy(&ffmpeg_process);
	if (subprocess_ret != 0) {
		return -1;
	}
	if (exit_code == 0) {
		int width, height;
		if (sscanf(resolution, "%dx%d\n", &width, &height) != 2) {
			return -1;
		}
		if (width_pt != NULL) {
			*width_pt = width;
		}
		if (height_pt != NULL) {
			*height_pt = height;
		}
		return 0;
	}
	else {
		return exit_code;
	}
}

int b2v_decode(const char *input, const char *output, int initial_block_size) {
	FILE *output_file;
	if (output == NULL) {
		output_file = stdout;
	}
	else {
		output_file = fopen(output, "wb");
		if (output_file == NULL) {
			perror("couldn't open output for writing");
			return EXIT_FAILURE;
		}
	}

	int real_width, real_height;
	if (video_resolution(input, &real_width, &real_height) != 0) {
		fprintf(stderr, "failed to get video resolution\n");
		return EXIT_FAILURE;
	}
	
	const char *argv[] = { "ffmpeg", "-i", input, "-f", "rawvideo", "-pix_fmt",
		"rgb24", "-v", "quiet", "-hide_banner", "-", NULL };
	struct subprocess_s ffmpeg_process;
	int subprocess_ret = spawn(argv, &ffmpeg_process, true);
	if (subprocess_ret != 0) {
		fprintf(stderr, "couldn't spawn ffmpeg\n");
		fclose(output_file);
		return EXIT_FAILURE;
	}

	int bit=0, byte=0;

	struct b2v_context ctx;
	b2v_context_init(&ctx, real_width / initial_block_size,
		real_height / initial_block_size, 1, initial_block_size);
	int blocks = ctx.width * ctx.height;

	int frame = 0;
	int result = -1;
	while (result == -1) {
		unsigned int read_ret = subprocess_read_stdout(&ffmpeg_process,
			(char *)ctx.image_scaled, blocks * ctx.scale * ctx.scale * 3);
		if (read_ret == 0) {
			result = EXIT_SUCCESS;
			break;
		}
		int ret = b2v_decode_image(&ctx);
		if (frame++ == 0) {
			// Metadata
			uint8_t metadata_version = ctx.buffer[0];
			if (metadata_version != METADATA_VERSION) {
				fprintf(stderr, "warning: unsupported metadata version (expected %d, "
					"got %d)\n", METADATA_VERSION, metadata_version);
			}
			ctx.scale = (int)ctx.buffer[1];
			ctx.bits_per_pixel = (int)ctx.buffer[2];
			uint8_t checksum = ctx.buffer[0] + ctx.buffer[1] + ctx.buffer[2];
			if (checksum != ctx.buffer[3]) {
				fprintf(stderr, "warning: corrupted metadata checksum\n");
			}
			ctx.width = real_width / ctx.scale;
			ctx.height = real_height / ctx.scale;
			b2v_context_realloc(&ctx);
			blocks = ctx.width * ctx.height;
		}
		else {
			// File data
			fwrite(ctx.buffer, 1, ret, output_file);
		}
	}

	b2v_context_destroy(&ctx);
	fclose(output_file);
	
	subprocess_terminate(&ffmpeg_process);
	subprocess_destroy(&ffmpeg_process);
	if (result == 0) {
		return ffmpeg_process.return_status;
	}
	else {
		return result;
	}
}

int b2v_encode(const char *input, const char *output, int real_width,
	int real_height, int initial_block_size, int block_size, int bits_per_pixel)
{
	FILE *input_file;
	if (input == NULL) {
		input_file = stdin;
	}
	else {
		input_file = fopen(input, "rb");
		if (input_file == NULL) {
			perror("couldn't open input for reading");
			return EXIT_FAILURE;
		}
	}

	struct subprocess_s ffmpeg_process;
	char video_resolution[33];
	snprintf(video_resolution, 33, "%dx%d", real_width, real_height);
	video_resolution[sizeof(video_resolution)-1] = 0;
	const char *argv[] = { "ffmpeg", "-framerate", "30", "-s", video_resolution,
		"-f", "rawvideo", "-pix_fmt", "rgb24", "-i", "-", "-c:v",
		"libx264", "-pix_fmt", "yuv420p", "-movflags", "+faststart",
		"-hide_banner", "-y", "-v", "quiet", "--", output, NULL };
	int subprocess_ret = spawn(argv, &ffmpeg_process, false);
	if ( subprocess_ret == -1 ) {
		fprintf(stderr, "couldn't spawn ffmpeg\n");
		fclose(input_file);
		return EXIT_FAILURE;
	}

	const int pixels = real_width * real_height;
	
	struct b2v_context ctx;
	b2v_context_init(&ctx, real_width / initial_block_size,
		real_height / initial_block_size, 1, initial_block_size);

	// Store metadata
	ctx.buffer[0] = METADATA_VERSION;
	ctx.buffer[1] = (uint8_t)block_size;
	ctx.buffer[2] = (uint8_t)bits_per_pixel;
	ctx.buffer[3] = ctx.buffer[0] + ctx.buffer[1] + ctx.buffer[2];
	ctx.bytes_available = 4;
	b2v_fill_image(&ctx);
	fwrite(ctx.image_scaled, pixels * 3, 1, ffmpeg_process.stdin_file);

	// Store file data
	ctx.bits_per_pixel = bits_per_pixel;
	ctx.scale = block_size;
	ctx.width = real_width / block_size;
	ctx.height = real_height / block_size;
	b2v_context_realloc(&ctx);
	while ( !feof(input_file) ) {
		b2v_fill_image_from_file(&ctx, input_file);
		fwrite(ctx.image_scaled, pixels * 3, 1, ffmpeg_process.stdin_file);
	}

	fclose(input_file);
	b2v_context_destroy(&ctx);

	int exit_code;
	subprocess_ret = subprocess_join(&ffmpeg_process, &exit_code);
	subprocess_destroy(&ffmpeg_process);
	if (subprocess_ret == 0) {
		return exit_code;
	}
	else {
		return subprocess_ret;
	}
}