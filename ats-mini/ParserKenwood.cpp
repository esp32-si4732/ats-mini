#include "ParserKenwood.h"
#include "ParserUtils.h"

using namespace command_parser;
using Id = KenwoodCommand::Id;
using Action = KenwoodCommand::Action;

namespace {
struct CommandSpec { const char* name; Id id; uint8_t digits; };
const CommandSpec commands[] = {
  {"FA", Id::Frequency, 11}, {"FB", Id::FrequencyB, 11}, {"MD", Id::Mode, 1},
  {"AG", Id::Volume, 3}, {"SQ", Id::Squelch, 3},
  {"AI", Id::AutoInformation, 1}, {"PS", Id::Power, 1},
  {"FR", Id::ReceiveVfo, 1}, {"FT", Id::TransmitVfo, 1},
  {"FW", Id::FilterWidth, 4}, {"SM", Id::SignalStrength, 0},
  {"ID", Id::Identity, 0}, {"IF", Id::Status, 0},
  {"RX", Id::Receive, 0}, {"UP", Id::StepUp, 2}, {"DN", Id::StepDown, 2},
  {"VV", Id::CopyVfo, 0}
};
}

static FramingDecision frameKenwood(ByteSpan input) {
  if (input.size == 1 && (newline(input.data[0]) || input.data[0] == ';')) return FramingDecision::Ignore;
  return kenwoodFraming.isTerminator(input.data[input.size - 1]) ? FramingDecision::Complete : FramingDecision::Incomplete;
}

const FramingRules command_parser::kenwoodFraming = {frameKenwood, [](uint8_t byte) { return byte == ';'; }};

ParseResult parseKenwood(ByteSpan frame, KenwoodCommand& out) {
  out = {};
  if (!frame.size || !kenwoodFraming.isTerminator(frame.data[frame.size - 1])) return ParseResult::InvalidSyntax;
  if (frame.size == 1) return ParseResult::None;
  if (frame.size < 3) return ParseResult::InvalidSyntax;
  for (size_t i = 0; i < frame.size - 1; ++i) {
    if (frame.data[i] < 0x21 || frame.data[i] > 0x7e || frame.data[i] == ';') return ParseResult::InvalidSyntax;
  }
  const ByteSpan name{frame.data, 2};
  ByteSpan value{frame.data + 2, frame.size - 3};
  const CommandSpec* spec = nullptr;
  for (const auto& command : commands) if (equal(name, command.name)) { spec = &command; break; }
  if (!spec) return ParseResult::UnknownCommand;
  out.id = spec->id;
  if (out.id == Id::Volume || out.id == Id::Squelch || out.id == Id::SignalStrength) {
    if (value.size != 1 && value.size != spec->digits + 1u) return ParseResult::InvalidSyntax;
    if (value.data[0] == '1') return ParseResult::Unsupported; // TS-2000 sub-receiver.
    if (value.data[0] != '0') return ParseResult::InvalidArgument;
    value = drop(value, 1);
  }
  const bool step = out.id == Id::StepUp || out.id == Id::StepDown;
  if (!value.size) {
    if (step || out.id == Id::Receive || out.id == Id::CopyVfo) {
      out.action = Action::Set;
      out.value = step ? 1 : 0;
    }
    return ParseResult::Ready;
  }
  if (!spec->digits || value.size != spec->digits) return ParseResult::InvalidSyntax;
  // At most 11 digits, so accumulation cannot overflow uint64_t.
  for (size_t i = 0; i < value.size; ++i) {
    if (value.data[i] < '0' || value.data[i] > '9') return ParseResult::InvalidArgument;
    out.value = out.value * 10 + value.data[i] - '0';
  }
  out.action = Action::Set;
  switch (out.id) {
    case Id::Mode:
      if (out.value == 0 || out.value == 8) return ParseResult::InvalidArgument;
      if (out.value != 1 && out.value != 2 && out.value != 4 && out.value != 5) return ParseResult::Unsupported;
      break;
    case Id::Volume:
    case Id::Squelch:
      if (out.value > 255) return ParseResult::OutOfRange;
      break;
    case Id::AutoInformation:
      if (out.value > 3) return ParseResult::InvalidArgument;
      break;
    case Id::Power:
      if (out.value == 9) return ParseResult::Unsupported;
      if (out.value > 1) return ParseResult::InvalidArgument;
      break;
    case Id::ReceiveVfo:
    case Id::TransmitVfo:
      if (out.value > 2) return ParseResult::InvalidArgument;
      if (out.value == 2) return ParseResult::Unsupported; // Memory channel selection.
      break;
    default: break;
  }
  return ParseResult::Ready;
}
