#include "../Parser.h"
#include "../ParserKenwood.h"
#include <cassert>
#include <cstring>
#include <initializer_list>

using Id = KenwoodCommand::Id;
using Action = KenwoodCommand::Action;
using command_parser::kenwoodFraming;

static ByteSpan bytes(const char* text) {
  return {reinterpret_cast<const uint8_t*>(text), std::strlen(text)};
}

static void testCommands() {
  const struct {
    const char* input;
    Id id;
    Action action;
    uint64_t value;
  } cases[] = {
    {"FA;", Id::Frequency, Action::Read, 0},
    {"FA00014074000;", Id::Frequency, Action::Set, 14074000},
    {"FB;", Id::FrequencyB, Action::Read, 0},
    {"fb00007074000;", Id::FrequencyB, Action::Set, 7074000},
    {"FB99999999999;", Id::FrequencyB, Action::Set, 99999999999ULL},
    {"FB00000000000;", Id::FrequencyB, Action::Set, 0},
    {"FR;", Id::ReceiveVfo, Action::Read, 0},
    {"FR0;", Id::ReceiveVfo, Action::Set, 0},
    {"FR1;", Id::ReceiveVfo, Action::Set, 1},
    {"FT;", Id::TransmitVfo, Action::Read, 0},
    {"FT0;", Id::TransmitVfo, Action::Set, 0},
    {"FT1;", Id::TransmitVfo, Action::Set, 1},
    {"MD;", Id::Mode, Action::Read, 0},
    {"MD1;", Id::Mode, Action::Set, 1},
    {"MD2;", Id::Mode, Action::Set, 2},
    {"MD4;", Id::Mode, Action::Set, 4},
    {"MD5;", Id::Mode, Action::Set, 5},
    {"IF;", Id::Status, Action::Read, 0},
    {"ID;", Id::Identity, Action::Read, 0},
    {"PS;", Id::Power, Action::Read, 0},
    {"PS0;", Id::Power, Action::Set, 0},
    {"PS1;", Id::Power, Action::Set, 1},
    {"AI;", Id::AutoInformation, Action::Read, 0},
    {"AI0;", Id::AutoInformation, Action::Set, 0},
    {"AI1;", Id::AutoInformation, Action::Set, 1},
    {"AI2;", Id::AutoInformation, Action::Set, 2},
    {"AI3;", Id::AutoInformation, Action::Set, 3},
    {"RX;", Id::Receive, Action::Set, 0},
    {"AG0;", Id::Volume, Action::Read, 0},
    {"AG0000;", Id::Volume, Action::Set, 0},
    {"AG0255;", Id::Volume, Action::Set, 255},
    {"SQ0;", Id::Squelch, Action::Read, 0},
    {"SQ0000;", Id::Squelch, Action::Set, 0},
    {"SQ0255;", Id::Squelch, Action::Set, 255},
    {"FW;", Id::FilterWidth, Action::Read, 0},
    {"FW0000;", Id::FilterWidth, Action::Set, 0},
    {"FW0001;", Id::FilterWidth, Action::Set, 1},
    {"FW0002;", Id::FilterWidth, Action::Set, 2},
    {"FW2400;", Id::FilterWidth, Action::Set, 2400},
    {"FW9999;", Id::FilterWidth, Action::Set, 9999},
    {"SM0;", Id::SignalStrength, Action::Read, 0},
    {"UP;", Id::StepUp, Action::Set, 1},
    {"UP00;", Id::StepUp, Action::Set, 0},
    {"UP99;", Id::StepUp, Action::Set, 99},
    {"DN;", Id::StepDown, Action::Set, 1},
    {"DN00;", Id::StepDown, Action::Set, 0},
    {"DN99;", Id::StepDown, Action::Set, 99},
    {"VV;", Id::CopyVfo, Action::Set, 0},
  };
  KenwoodCommand command;
  for (const auto& expected : cases) {
    assert(parseKenwood(bytes(expected.input), command) == ParseResult::Ready);
    assert(command.id == expected.id);
    assert(command.action == expected.action);
    assert(command.value == expected.value);
  }
}

static void testInvalidCommands() {
  KenwoodCommand command;
  for (const char* input : {"", "F;", "FB", "FB123;", "FB000000000000;",
                            "FR01;", "FT01;", "FW000;", "FW00000;", "VV0;",
                            "SM;", "SM00;", "AG;", "SQ00;", "UP1;", "DN100;",
                            "RX0;", "ID0;", "IF0;", "F B;", "FB\r;", "FA;FB;"}) {
    assert(parseKenwood(bytes(input), command) == ParseResult::InvalidSyntax);
  }
  for (const char* input : {"FB0000707400x;", "FW24x0;", "FW-001;", "FR3;", "FT3;",
                            "FRx;", "FTx;", "MD0;", "MD8;", "PS2;", "AI4;", "AI9;",
                            "AG2;", "SQ2;", "SM2;", "UPxx;", "DN-1;"}) {
    assert(parseKenwood(bytes(input), command) == ParseResult::InvalidArgument);
  }
  for (const char* input : {"FR2;", "FT2;", "MD3;", "MD6;", "MD7;", "MD9;",
                            "PS9;", "AG1;", "SQ1;", "SM1;"}) {
    assert(parseKenwood(bytes(input), command) == ParseResult::Unsupported);
  }
  for (const char* input : {"AG0256;", "SQ0256;", "SQ0999;"})
    assert(parseKenwood(bytes(input), command) == ParseResult::OutOfRange);
  assert(parseKenwood(bytes("ZZ;"), command) == ParseResult::UnknownCommand);
  assert(parseKenwood(bytes("TX;"), command) == ParseResult::UnknownCommand);
  assert(parseKenwood(bytes(";"), command) == ParseResult::None);
}

static void testStreaming() {
  FramerState state;
  ByteSpan frame;
  KenwoodCommand command;
  const Id expected[] = {Id::FrequencyB, Id::ReceiveVfo, Id::TransmitVfo,
                         Id::FilterWidth, Id::CopyVfo};
  size_t count = 0;
  for (const char byte : "\r\n;FB00007074000;FR1;\r\nFT0;FW2400;VV;") {
    if (!byte) break;
    const FrameEvent event = feed(state, kenwoodFraming, byte, frame);
    if (event == FrameEvent::None) continue;
    assert(event == FrameEvent::Ready);
    assert(count < sizeof(expected) / sizeof(expected[0]));
    // Consume the borrowed frame before feeding the next byte.
    assert(parseKenwood(frame, command) == ParseResult::Ready);
    assert(command.id == expected[count++]);
  }
  assert(count == sizeof(expected) / sizeof(expected[0]));
  assert(state.length == 0);
}

int main() {
  testCommands();
  testInvalidCommands();
  testStreaming();
}
