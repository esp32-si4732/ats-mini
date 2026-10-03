#ifndef PARSER_TYPES_H
#define PARSER_TYPES_H

#include <cstddef>
#include <cstdint>

enum class ParseResult : uint8_t {
  None, // No event: partial input, ignored bytes, or ongoing discard.
  Ready, BlankFrame, DiscardComplete, FrameTooLong, UnknownCommand,
  InvalidSyntax,   // Wrong framing, argument count, or required command keywords.
  InvalidArgument, // Malformed, oversized, or out-of-range argument value.
  OutOfRange, Unsupported
};

struct ByteSpan {
  const uint8_t* data;
  size_t size;
  ByteSpan(const uint8_t* data = nullptr, size_t size = 0) : data(data), size(size) {}
};

namespace command_parser {
// Framers inspect nonempty input without changing parser state or output.
enum class FramingDecision { Incomplete, Complete, BlankFrame, Ignore, RestartAtCurrentByte };
}

// Inspect receives nonempty buffered input. A null terminator means recovery
// discards until reset(), suitable for protocols without a delimiter.
struct FramingRules {
  command_parser::FramingDecision (*inspect)(ByteSpan input);
  bool (*isTerminator)(uint8_t byte);
};

#endif
