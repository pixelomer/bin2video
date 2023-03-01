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

int get_bit(uint8_t *buffer, int size, int *tbyte, int *tbit, int *idx) {
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
}

void b2v_context_init(struct b2v_context *ctx, int width, int height,
	int bits_per_pixel, int scale)
{
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

int b2v_fill_image(struct b2v_context *ctx) {
	int bits_per_comp[3];
	double div[3];
	for (int i=0; i<3; i++) {
		bits_per_comp[i] = ctx->bits_per_pixel / 3;
		if ((ctx->bits_per_pixel % 3) > i) {
			bits_per_comp[i] += 1;
		}
		div[i] = 255.0 / (double)((1 << bits_per_comp[i]) - 1);
	}

	if (ctx->tbyte == -1) {
		ctx->tbyte = 0;
	}
	int buffer_idx = 0;
	int image_idx=0;
	int blocks = ctx->width * ctx->height;
	for (image_idx=0; (image_idx < blocks) && (ctx->tbyte != -1); image_idx++) {
		switch (ctx->bits_per_pixel) {
			int value;
			case 1:
				value = get_bit(ctx->buffer, ctx->bytes_available, &ctx->tbyte, &ctx->tbit,
					&buffer_idx) * 0xFF;
				memset(ctx->image + (image_idx * 3), value, 3);
				break;
			default:
				for (int c=0; c<3; c++) {
					value = 0;
					for (int b=0; b<bits_per_comp[c]; b++) {
						value <<= 1;
						value |= get_bit(ctx->buffer, ctx->bytes_available, &ctx->tbyte,
							&ctx->tbit, &buffer_idx);
					}
					value = (uint8_t)round(div[c] * (double)value);
					ctx->image[image_idx * 3 + c] = value;
				}
				break;
		}
	}
	memset(ctx->image + image_idx * 3, 0, (blocks - image_idx) * 3);
	scale_up(ctx->image, ctx->image_scaled, ctx->width, ctx->height, ctx->scale);

	return buffer_idx;
}

void b2v_fill_image_from_file(struct b2v_context *ctx, FILE *file) {
	ctx->bytes_available += fread(ctx->buffer + ctx->bytes_available, 1,
		ctx->buffer_size - ctx->bytes_available, file);
	int next_idx = b2v_fill_image(ctx);
	memmove(ctx->buffer, ctx->buffer + next_idx, ctx->bytes_available - next_idx);
	ctx->bytes_available -= next_idx;
}

int b2v_decode_image(struct b2v_context *ctx) {
	int bits_per_comp[3];
	double div[3];
	for (int i=0; i<3; i++) {
		bits_per_comp[i] = ctx->bits_per_pixel / 3;
		if ((ctx->bits_per_pixel % 3) > i) {
			bits_per_comp[i] += 1;
		}
		div[i] = 255.0 / (double)((1 << bits_per_comp[i]) - 1);
	}

	scale_down(ctx->image_scaled, ctx->image, ctx->width, ctx->height,
		ctx->scale);
	int buffer_idx = 0;
	int blocks = ctx->width * ctx->height;
	for (int i=0; i<blocks; i++) {
		switch (ctx->bits_per_pixel) {
			int value;
			case 1:
				value = ((int)ctx->image[i * 3] + (int)ctx->image[i * 3 + 1]
					+ (int)ctx->image[i * 3 + 2]) / 3;
				value = (value > 127) ? 1 : 0;
				put_bit(ctx->buffer, value, &ctx->tbyte, &ctx->tbit, &buffer_idx);
				break;
			default:
				for (int j=0; j<3; j++) {
					double color = (double)ctx->image[i * 3 + j];
					value = (int)round(color / div[j]);
					for (int b=bits_per_comp[j]-1; b>=0; b--) {
						put_bit(ctx->buffer, ((uint8_t)value >> b) & 1, &ctx->tbyte,
							&ctx->tbit, &buffer_idx);
					}
				}
				break;
		}
	}

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
		"-show_entries", "stream=width,height", "-of", "csv=s=x:p=0", file, NULL };
	
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

int b2v_decode(const char *input, const char *output, int bits_per_pixel) {
	FILE *output_file = fopen(output, "wb");
	if (output_file == NULL) {
		perror("couldn't open output for writing");
		return EXIT_FAILURE;
	}

	int real_width, real_height;
	if (video_resolution(input, &real_width, &real_height) != 0) {
		fprintf(stderr, "failed to get video resolution\n");
		return EXIT_FAILURE;
	}
	
	const char *argv[] = { "ffmpeg", "-i", (char *)input, "-f", "rawvideo", "-pix_fmt",
		"rgb24", "-", "-v", "quiet", "-hide_banner", NULL };
	struct subprocess_s ffmpeg_process;
	int subprocess_ret = spawn(argv, &ffmpeg_process, true);
	if (subprocess_ret != 0) {
		fprintf(stderr, "couldn't spawn ffmpeg\n");
		fclose(output_file);
		return EXIT_FAILURE;
	}

	int bit=0, byte=0;

	const int scale = VIDEO_SCALE;
	const int width = INTERNAL_WIDTH;
	const int height = INTERNAL_HEIGHT;
	const int pixels = width * height;

	int bits_per_comp[3];
	double div[3];
	for (int i=0; i<3; i++) {
		bits_per_comp[i] = bits_per_pixel / 3;
		if ((bits_per_pixel % 3) > i) {
			bits_per_comp[i] += 1;
		}
		div[i] = 255.0 / (double)((1 << bits_per_comp[i]) - 1);
	}

	struct b2v_context ctx;
	b2v_context_init(&ctx, width, height, bits_per_pixel, scale);

	int result = -1;
	while (result == -1) {
		unsigned int read_ret = subprocess_read_stdout(&ffmpeg_process,
			(char *)ctx.image_scaled, pixels * scale * scale * 3);
		if (read_ret == 0) {
			result = EXIT_SUCCESS;
			break;
		}
		int ret = b2v_decode_image(&ctx);
		fwrite(ctx.buffer, 1, ret, output_file);
	}

	b2v_context_destroy(&ctx);
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

int b2v_encode(const char *input, const char *output, int real_width,
	int real_height, int initial_block_size, int block_size, int bits_per_pixel)
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
	const char *argv[] = { "ffmpeg", "-framerate", "30", "-s", VIDEO_RESOLUTION, "-f",
		"rawvideo", "-pix_fmt", "rgb24", "-i", "-", "-c:v",
		"libx264", "-pix_fmt", "yuv420p", "-movflags", "+faststart", (char *)output,
		"-hide_banner", "-y", "-v", "quiet", NULL };
	int subprocess_ret = spawn(argv, &ffmpeg_process, false);
	if ( subprocess_ret == -1 ) {
		fprintf(stderr, "couldn't spawn ffmpeg\n");
		fclose(input_file);
		return EXIT_FAILURE;
	}

	FILE *ffmpeg_stdin = ffmpeg_process.stdin_file;

	int current_block_size = block_size;
	int width = real_width / current_block_size;
	int height = real_height / current_block_size;
	const int blocks = width * height;
	const int pixels = real_width * real_height;
	
	struct b2v_context ctx;
	b2v_context_init(&ctx, width, height, bits_per_pixel, block_size);

	while ( !feof(input_file) ) {
		b2v_fill_image_from_file(&ctx, input_file);
		fwrite(ctx.image_scaled, pixels * 3, 1, ffmpeg_stdin);
	}

	fclose(input_file);
	fclose(ffmpeg_stdin);
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