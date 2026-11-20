#ifndef PARSER_KENWOOD_H
#define PARSER_KENWOOD_H

#include "ParserTypes.h"

// Receiver CAT subset using common TS-480/590/2000 command forms.
// FA, FB, MD, AG0, SQ0, AI, PS, FR, FT, FW: read/set;
// SM0, ID, IF: read; RX, UP, DN, VV: set.
// One receiver, VFO A/B. MD supports 1=LSB, 2=USB, 4=FM, 5=AM; AI supports 0..3.
struct KenwoodCommand {
  enum class Id : uint8_t {
    Unknown, Frequency, Mode, Volume, Squelch, AutoInformation, Power,
    ReceiveVfo, SignalStrength, Identity, Status, Receive, StepUp, StepDown,
    FrequencyB, TransmitVfo, FilterWidth, CopyVfo
  };
  enum class Action : uint8_t { Read, Set };
  Id id;
  Action action;
  // Frequency (FA)/FrequencyB: all 11 decimal Hz digits; Mode: CAT code;
  // Volume/Squelch: 0..255; AI: 0..3; Power: 0/1; ReceiveVfo/TransmitVfo: 0=A, 1=B;
  // FilterWidth: raw 0000..9999; StepUp/StepDown: 0..99 steps (default 1).
  // Read commands, RX and VV have no value. Handlers validate receiver limits,
  // receive-only FT behavior, and apply scaling. FW is a compatibility no-op.
  uint64_t value;
};

// Exactly one semicolon-terminated command; command letters are case-insensitive.
// A bare ';' is a no-op. CR/LF between streamed commands are ignored, not within one.
// AG/SQ values above 255 are rejected.
// No command execution, reply formatting, or automatic-information scheduling.
// On errors the output may be partially filled and must not be executed.
ParseResult parseKenwood(ByteSpan frame, KenwoodCommand& out);
namespace command_parser {
extern const FramingRules kenwoodFraming;
}

#endif
