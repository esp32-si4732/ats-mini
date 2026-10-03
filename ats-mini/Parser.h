#ifndef PARSER_H
#define PARSER_H

#include "ParserTypes.h"

struct FramerState {
  static constexpr size_t BUFFER_SIZE = 256;
  uint8_t buffer[BUFFER_SIZE] = {};
  uint16_t length = 0;
  bool discarding = false;
  bool ignoring = false; // Finish the frame without delivering it.
};

enum class FrameEvent : uint8_t { None, Ready, BlankFrame, DiscardComplete, FrameTooLong };

void reset(FramerState& state);
// Suppress delivery while framing the current input.
// Complete or BlankFrame reports DiscardComplete; Ignore clears suppression;
// RestartAtCurrentByte preserves it. Overflow recovery remains silent.
void ignoreCurrentFrame(FramerState& state);
// Abandon input through the framing policy's terminator. For binary protocols
// without a delimiter, use reset() to begin again at a known packet boundary.
void discardFrame(FramerState& state);
// A Ready event exposes a borrowed view into state.buffer. Decode it before
// the next feed() call. Other events clear frame. Changing framing requires reset().
FrameEvent feed(FramerState& state, const FramingRules& framing, uint8_t byte, ByteSpan& frame);

#endif
