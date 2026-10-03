#ifndef PARSER_UTILS_H
#define PARSER_UTILS_H

#include "ParserTypes.h"

namespace command_parser {

inline bool space(uint8_t c) { return c == ' ' || c == '\t'; }
inline bool newline(uint8_t c) { return c == '\r' || c == '\n'; }

inline bool lineFeed(uint8_t byte) { return byte == '\n'; }

// Validate a complete line using the protocol's terminator rule, remove its
// ending (including CR before LF), and reject embedded CR/LF. The view is borrowed.
inline bool stripLineEnding(ByteSpan frame, bool (*isTerminator)(uint8_t), ByteSpan& text) {
  if (frame.size == 0 || !isTerminator(frame.data[frame.size - 1])) return false;
  text = {frame.data, frame.size - 1};
  if (frame.data[frame.size - 1] == '\n' && text.size && text.data[text.size - 1] == '\r') --text.size;
  for (size_t i = 0; i < text.size; ++i) if (newline(text.data[i])) return false;
  return true;
}

// blank determines whether whitespace-only lines are reported, ignored, or
// decoded as commands. Invalid completed lines go to the decoder for an error.
inline FramingDecision frameLine(ByteSpan input, bool (*isTerminator)(uint8_t), FramingDecision blank) {
  if (input.size == 0 || !isTerminator(input.data[input.size - 1])) return FramingDecision::Incomplete;
  if (blank == FramingDecision::Complete) return FramingDecision::Complete;
  ByteSpan text;
  if (!stripLineEnding(input, isTerminator, text)) return FramingDecision::Complete;
  for (size_t i = 0; i < text.size; ++i) {
    if (!space(text.data[i])) return FramingDecision::Complete;
  }
  return blank;
}

} // namespace command_parser
#endif
