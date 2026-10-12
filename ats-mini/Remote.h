#ifndef REMOTE_H
#define REMOTE_H

#include <Arduino.h>
#include "Parser.h"

enum class InputEvent : uint8_t { NoData, SessionReset, Byte };

// Nonblocking. Report SessionReset before any bytes from a new connection,
// even when no bytes are queued. Only Byte sets the output argument.
using RemoteReadInput = InputEvent (*)(uint8_t& byte);

struct RemoteState {
  uint32_t remoteTimer = millis();
  uint8_t remoteSeqnum = 0;
  bool remoteLogOn = false;
  FramerState input;
  uint32_t inputTimer = 0;
};

// Capture from the main loop into a buffer of at least BMP_FILE_SIZE bytes.
bool remoteCaptureBmp(uint8_t *bmp);

// Process at most one command. Return whether the display needs updating.
bool remoteLoop(Stream* stream, RemoteState* state, RemoteReadInput readInput);
// Skip ignored separators; consume cancellation input without executing its
// command, including its tail.
bool remoteConsumeAbortPending(RemoteState* state, RemoteReadInput readInput);
bool serialLoop(uint8_t usbMode);
bool serialConsumeAbortPending(uint8_t usbMode);

#endif
