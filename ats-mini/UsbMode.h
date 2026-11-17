#ifndef USB_MODE_H
#define USB_MODE_H

#include <stdint.h>

bool usbLoop(uint8_t usbMode);
bool usbConsumeAbortPending(uint8_t usbMode);

#endif
