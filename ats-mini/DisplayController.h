#ifndef DISPLAY_CONTROLLER_H
#define DISPLAY_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

void displayInit(void);
void displaySleep(void);
void displayWake(void);
void displaySetBrightness(uint16_t level);
bool displayIsAwake(void);

#endif
