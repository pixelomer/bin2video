#ifndef B2V_STBI_IMAGE_WRITE_FD_H
#define B2V_STBI_IMAGE_WRITE_FD_H

int stbi_write_png_to_fd(int fd, int w, int h, int comp, const void *data,
	int stride_in_bytes);

#endif