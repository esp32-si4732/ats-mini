#ifndef PARSER_UTILS_H
#define PARSER_UTILS_H

#include "ParserTypes.h"
#include <cstring>

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

inline uint8_t lower(uint8_t c) { return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c; }

inline ByteSpan drop(ByteSpan s, size_t n) {
  if (n == 0) return s;
  return n <= s.size ? ByteSpan{s.data + n, s.size - n} : ByteSpan{};
}

template <size_t N>
bool copy(ByteSpan s, char (&out)[N]) {
  if (s.size >= N) return false;
  // Command strings are C strings, so embedded NUL bytes are invalid.
  for (size_t i = 0; i < s.size; ++i) if (s.data[i] == 0) return false;
  if (s.size != 0) std::memcpy(out, s.data, s.size);
  out[s.size] = '\0';
  return true;
}

inline bool unsignedNumber(ByteSpan s, uint32_t& out, uint32_t limit = UINT32_MAX) {
  if (s.size == 0) return false;
  uint32_t value = 0;
  for (size_t i = 0; i < s.size; ++i) {
    if (s.data[i] < '0' || s.data[i] > '9') return false;
    const uint32_t digit = s.data[i] - '0';
    if (digit > limit || value > (limit - digit) / 10) return false;
    value = value * 10 + digit;
  }
  out = value;
  return true;
}

inline bool hexNumber(ByteSpan s, uint32_t& out) {
  if (s.size == 0 || s.size > 8) return false;
  uint32_t value = 0;
  for (size_t i = 0; i < s.size; ++i) {
    const uint8_t c = lower(s.data[i]);
    if (c >= '0' && c <= '9') value = value * 16 + c - '0';
    else if (c >= 'a' && c <= 'f') value = value * 16 + c - 'a' + 10;
    else return false;
  }
  out = value;
  return true;
}

struct Cursor {
  ByteSpan rest;
  explicit Cursor(ByteSpan s) : rest(s) {}

  bool field(uint8_t delimiter, ByteSpan& out) {
    size_t n = 0;
    while (n < rest.size && rest.data[n] != delimiter) ++n;
    if (n == rest.size) return false;
    out = {rest.data, n};
    rest = drop(rest, n + 1);
    return true;
  }
};

} // namespace command_parser
#endif
