#ifndef EVENT_HANDLER_H
#define EVENT_HANDLER_H

#include <stdint.h>

// Consume encoder counts and dispatch tuning/seek commands.
// Called once per loop() iteration. Returns true if screen needs redraw.
bool handleEncoderInput(void);

#endif
