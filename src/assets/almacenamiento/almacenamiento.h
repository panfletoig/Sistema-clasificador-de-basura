#pragma once
#include <stdint.h>

void mount_sd(void);
void save_as_bmp(const char *filename, uint8_t *rgb, int width, int height);