#ifndef REMOTE_ADHOC_H
#define REMOTE_ADHOC_H

#include <Arduino.h>
#include "Parser.h"

struct AdhocState {
  uint32_t reportTimer = millis();
  uint8_t sequence = 0;
  bool logging = false;
};

// Consume the borrowed frame synchronously; return whether the display needs updating.
bool remoteAdhocFrame(Stream& stream, AdhocState& state, ByteSpan frame);
void remoteAdhocTick(Stream& stream, AdhocState& state);
void remoteAdhocEcho(Stream& stream, const FramerState& input, uint8_t byte);
// Inspect timeout input before the session discards it.
void remoteAdhocTimeout(Stream& stream, const FramerState& input);
void remoteAdhocOverflow(Stream& stream);

#endif
