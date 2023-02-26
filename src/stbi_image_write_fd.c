#include <stdio.h>
#include <unistd.h>
#include "stb_image_write.h"
#include "stbi_image_write_fd.h"

void _stbi_write_png_to_fd_callback(void *context, void *data, int size) {
	int *fd = (int *)context;
	if (*fd == -1) {
		return;
	}
	int ret = write(*fd, data, size);
	if (ret == -1) {
		perror("failed to write image data to file");
		*fd = -1;
	}
	else if (ret != size) {
		fprintf(stderr, "attempted to write %d bytes to file, only wrote %d",
			size, ret);
		*fd = -1;
	}
}

int stbi_write_png_to_fd(int fd, int w, int h, int comp, const void *data,
	int stride_in_bytes)
{
	int ret = stbi_write_png_to_func(_stbi_write_png_to_fd_callback, &fd, w, h, comp,
		data, stride_in_bytes);
	if (fd < 0) {
		return -1;
	}
	return ret;
}