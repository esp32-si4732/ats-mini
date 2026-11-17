#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <Arduino.h>

// Capture from the main loop into a buffer of at least BMP_FILE_SIZE bytes.
bool screenshotCaptureBmp(uint8_t* bmp);
// Write a binary or hex-encoded BMP to a remote stream.
void screenshotWrite(Stream& stream, bool binary);

#endif
