#ifndef REMOTE_KENWOOD_H
#define REMOTE_KENWOOD_H

#include <Arduino.h>
#include "ParserTypes.h"

struct KenwoodState {
  uint32_t reportTimer = millis();
  uint8_t autoInformation = 0; // Bit 0: legacy IF; bit 1: extended parameter reports.
  bool statusPending = false;
  uint32_t lastFrequencyA = 0;
  uint32_t lastFrequencyB = 0;
  uint8_t lastMode = 0; // CAT modes start at 1; zero means no report yet.
  uint8_t lastVfo = 0xff;
  uint8_t lastVolume = 0xff;
  uint8_t lastSquelch = 0xff;
};

// Consume the borrowed frame synchronously; return whether the display needs updating.
bool remoteKenwoodFrame(Stream& stream, KenwoodState& state, ByteSpan frame);
void remoteKenwoodTick(Stream& stream, KenwoodState& state);
void remoteKenwoodError(Stream& stream);

#endif
