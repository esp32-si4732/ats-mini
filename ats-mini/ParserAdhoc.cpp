#include "ParserAdhoc.h"
#include "ParserUtils.h"

using namespace command_parser;

static FramingDecision frameAdhoc(ByteSpan input) {
  const uint8_t first = input.data[0];

  // Memory and frequency commands end at CR or LF.
  if (first == '#' || first == 'F') {
    const uint8_t last = input.data[input.size - 1];
    if (!newline(last)) return FramingDecision::Incomplete;
    return FramingDecision::Complete;
  }

  // A theme is '^' followed by one xHHHH group per color, with no terminator.
  if (first == '^') {
    // End a truncated theme at a newline so the decoder rejects it and input can resume.
    if (newline(input.data[input.size - 1])) return FramingDecision::Complete;
    const size_t frameLength = 1 + AdhocCommand::THEME_WORDS * 5;
    if (input.size < frameLength) return FramingDecision::Incomplete;
    return FramingDecision::Complete;
  }

  // Ignore line endings between commands; all other bytes complete immediately.
  if (newline(first)) return FramingDecision::Ignore;
  return FramingDecision::Complete;
}

const FramingRules command_parser::adhocFraming = {frameAdhoc, newline};

ParseResult parseAdhoc(ByteSpan frame, AdhocCommand& out) {
  out = {};
  using Id = AdhocCommand::Id;
  if (frame.size == 0) return ParseResult::InvalidSyntax;

  if (frame.data[0] == 'F') {
    ByteSpan text;
    if (!stripLineEnding(frame, adhocFraming.isTerminator, text)) return ParseResult::InvalidSyntax;
    if (text.size == 1) return ParseResult::InvalidSyntax;
    if (!unsignedNumber(drop(text, 1), out.frequency) || out.frequency == 0) {
      return ParseResult::InvalidArgument;
    }
    out.id = Id::SetFrequency;
    return ParseResult::Ready;
  }

  if (frame.data[0] == '#') {
    ByteSpan text, slot, band, frequency_text;
    if (!stripLineEnding(frame, adhocFraming.isTerminator, text)) return ParseResult::InvalidSyntax;
    Cursor cursor(drop(text, 1));
    if (!cursor.field(',', slot) || !cursor.field(',', band) || !cursor.field(',', frequency_text)) {
      return ParseResult::InvalidSyntax;
    }
    if (!unsignedNumber(slot, out.memory_slot) || out.memory_slot == 0) return ParseResult::InvalidArgument;
    if (!unsignedNumber(frequency_text, out.frequency)) return ParseResult::InvalidArgument;
    ByteSpan mode;
    out.has_name = cursor.field(',', mode);
    if (!out.has_name) mode = cursor.rest;
    if (band.size == 0 || mode.size == 0) return ParseResult::InvalidSyntax;
    if (!copy(band, out.band) || !copy(mode, out.mode)) return ParseResult::InvalidArgument;
    if (out.has_name) {
      for (size_t i = 0; i < cursor.rest.size; ++i) {
        if (cursor.rest.data[i] < 0x20 || cursor.rest.data[i] > 0x7e) return ParseResult::InvalidArgument;
      }
      if (!copy(cursor.rest, out.name)) return ParseResult::InvalidArgument;
    }
    out.id = Id::SetMemory;
    return ParseResult::Ready;
  }

  if (frame.data[0] == '^') {
    if (frame.size != 1 + AdhocCommand::THEME_WORDS * 5) return ParseResult::InvalidSyntax;
    for (size_t i = 0; i < AdhocCommand::THEME_WORDS; ++i) {
      const size_t offset = 1 + i * 5;
      if (frame.data[offset] != 'x') return ParseResult::InvalidSyntax;
      uint32_t color;
      if (!hexNumber({frame.data + offset + 1, 4}, color)) return ParseResult::InvalidArgument;
      out.theme_words[i] = uint16_t(color);
    }
    out.id = Id::SetTheme;
    return ParseResult::Ready;
  }

  if (frame.size != 1) return ParseResult::InvalidSyntax;
  switch (frame.data[0]) {
    case 'R': out.id = Id::RotateCw; break;
    case 'r': out.id = Id::RotateCcw; break;
    case 'e': out.id = Id::Click; break;
    case 'E': out.id = Id::ShortPress; break;
    case 'B': out.id = Id::BandUp; break;
    case 'b': out.id = Id::BandDown; break;
    case 'M': out.id = Id::ModeUp; break;
    case 'm': out.id = Id::ModeDown; break;
    case 'S': out.id = Id::StepUp; break;
    case 's': out.id = Id::StepDown; break;
    case 'W': out.id = Id::BandwidthUp; break;
    case 'w': out.id = Id::BandwidthDown; break;
    case 'A': out.id = Id::AgcUp; break;
    case 'a': out.id = Id::AgcDown; break;
    case 'V': out.id = Id::VolumeUp; break;
    case 'v': out.id = Id::VolumeDown; break;
    case 'L': out.id = Id::BacklightUp; break;
    case 'l': out.id = Id::BacklightDown; break;
    case 'O': out.id = Id::SleepOn; break;
    case 'o': out.id = Id::SleepOff; break;
    case 'I': out.id = Id::CalUp; break;
    case 'i': out.id = Id::CalDown; break;
    case 'C': out.id = Id::CaptureScreen; break;
    case 'c': out.id = Id::CaptureScreenBinary; break;
    case 't': out.id = Id::ToggleLog; break;
    case '$': out.id = Id::GetMemories; break;
    case 'T': out.id = Id::ToggleThemeEditor; break;
    case '@': out.id = Id::GetTheme; break;
    case '\r': case '\n': return ParseResult::None;
    default: return ParseResult::UnknownCommand;
  }
  return ParseResult::Ready;
}
