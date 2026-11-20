#ifndef REMOTE_H
#define REMOTE_H

#include <Arduino.h>
#include "Parser.h"
#include "RemoteAdhoc.h"
#include "RemoteKenwood.h"

enum class RemoteProtocol : uint8_t { Adhoc, Kenwood };

enum class InputEvent : uint8_t { NoData, SessionReset, Byte };

// Nonblocking. Report SessionReset before any bytes from a new connection,
// even when no bytes are queued. Only Byte sets the output argument.
using RemoteReadInput = InputEvent (*)(uint8_t& byte);

struct RemoteState {
  FramerState input;
  uint32_t inputTimer = 0;
  AdhocState adhoc;
  KenwoodState kenwood;
};

// Process at most one command. Return whether the display needs updating.
bool remoteLoop(Stream& stream, RemoteState& state, RemoteReadInput readInput,
                RemoteProtocol protocol = RemoteProtocol::Adhoc);
// Skip ignored separators; consume cancellation input without executing its
// command, including its tail.
bool remoteConsumeAbortPending(RemoteState& state, RemoteReadInput readInput,
                               RemoteProtocol protocol = RemoteProtocol::Adhoc);

#endif
