#include "../Parser.h"
#include "../ParserAdhoc.h"
#include <cassert>
#include <cstring>
#include <initializer_list>
#include <string>

using AdhocId = AdhocCommand::Id;
using command_parser::adhocFraming;

// The view is used only during the immediately following parser call.
static ByteSpan bytes(const std::string& text) {
  return {reinterpret_cast<const uint8_t*>(text.data()), text.size()};
}

static void testSingleByteCommands() {
  const struct {
    uint8_t byte;
    AdhocId id;
  } commands[] = {
    {'R', AdhocId::RotateCw}, {'r', AdhocId::RotateCcw},
    {'e', AdhocId::Click}, {'E', AdhocId::ShortPress},
    {'B', AdhocId::BandUp}, {'b', AdhocId::BandDown},
    {'M', AdhocId::ModeUp}, {'m', AdhocId::ModeDown},
    {'S', AdhocId::StepUp}, {'s', AdhocId::StepDown},
    {'W', AdhocId::BandwidthUp}, {'w', AdhocId::BandwidthDown},
    {'A', AdhocId::AgcUp}, {'a', AdhocId::AgcDown},
    {'V', AdhocId::VolumeUp}, {'v', AdhocId::VolumeDown},
    {'L', AdhocId::BacklightUp}, {'l', AdhocId::BacklightDown},
    {'O', AdhocId::SleepOn}, {'o', AdhocId::SleepOff},
    {'I', AdhocId::CalUp}, {'i', AdhocId::CalDown},
    {'C', AdhocId::CaptureScreen}, {'c', AdhocId::CaptureScreenBinary},
    {'t', AdhocId::ToggleLog}, {'$', AdhocId::GetMemories},
    {'T', AdhocId::ToggleThemeEditor}, {'@', AdhocId::GetTheme},
  };
  FramerState state;
  ByteSpan frame;
  AdhocCommand command;

  for (const auto& expected : commands) {
    assert(feed(state, adhocFraming, expected.byte, frame) == FrameEvent::Ready);
    assert(parseAdhoc(frame, command) == ParseResult::Ready);
    assert(command.id == expected.id);
  }
  assert(parseAdhoc(bytes("?"), command) == ParseResult::UnknownCommand);
  assert(parseAdhoc(bytes("RR"), command) == ParseResult::InvalidSyntax);
  assert(parseAdhoc(bytes(""), command) == ParseResult::InvalidSyntax);
  assert(parseAdhoc(bytes("\r"), command) == ParseResult::None);
  assert(parseAdhoc(bytes("\n"), command) == ParseResult::None);
}

static void testFrequency() {
  AdhocCommand command;
  for (const char* ending : {"\r", "\n", "\r\n"}) {
    assert(parseAdhoc(bytes(std::string("F7100123") + ending), command) == ParseResult::Ready);
    assert(command.id == AdhocId::SetFrequency);
    assert(command.frequency == 7100123);
  }
  assert(parseAdhoc(bytes("F1\r"), command) == ParseResult::Ready);
  assert(command.frequency == 1);
  assert(parseAdhoc(bytes("F2147483647\r"), command) == ParseResult::Ready);
  assert(command.frequency == INT32_MAX);
  assert(parseAdhoc(bytes("F2147483648\r"), command) == ParseResult::Ready);
  assert(command.frequency == 2147483648u);
  assert(parseAdhoc(bytes("F4294967295\r"), command) == ParseResult::Ready);
  assert(command.frequency == UINT32_MAX);

  for (const char* input : {"F0\r", "F-1\r", "F+1\r", "F1m\r", "F 1\r", "F1 \r",
                            "F4294967296\r"}) {
    assert(parseAdhoc(bytes(input), command) == ParseResult::InvalidArgument);
  }
  assert(parseAdhoc(bytes("F\r"), command) == ParseResult::InvalidSyntax);
  assert(parseAdhoc(bytes("F123"), command) == ParseResult::InvalidSyntax);
  assert(parseAdhoc(bytes("F1\r2\n"), command) == ParseResult::InvalidSyntax);
}

static void testMemory() {
  AdhocCommand command;
  assert(parseAdhoc(bytes("#12,SW,7253500,USB\r"), command) == ParseResult::Ready);
  assert(command.id == AdhocId::SetMemory);
  assert(command.memory_slot == 12);
  assert(std::strcmp(command.band, "SW") == 0);
  assert(command.frequency == 7253500);
  assert(std::strcmp(command.mode, "USB") == 0);
  assert(!command.has_name);

  for (const char* name : {"Radio One", "A,B! C", ""}) {
    assert(parseAdhoc(bytes(std::string("#01,VHF,107900000,FM,") + name + "\r"), command) == ParseResult::Ready);
    assert(command.has_name);
    assert(std::strcmp(command.name, name) == 0);
  }
  // No name preserves it; an explicit zero frequency clears the slot.
  assert(parseAdhoc(bytes("#1,MW,0,AM\r"), command) == ParseResult::Ready);
  assert(!command.has_name && command.name[0] == '\0');
  assert(command.frequency == 0);

  // Syntax parsing leaves slot limits, band/mode lookup, and tuning ranges to the handler.
  assert(parseAdhoc(bytes("#100,UNKNOWN,4294967295,XYZ\r"), command) == ParseResult::Ready);
  assert(command.memory_slot == 100 && command.frequency == UINT32_MAX);
}

static void testInvalidMemory() {
  AdhocCommand command;
  for (const char* input : {"#1,MW,100\r", "#1,MW,100,\r", "#1,MW,100,AM",
                            "#1,,100,AM\r", "#1,,0,AM\r"}) {
    assert(parseAdhoc(bytes(input), command) == ParseResult::InvalidSyntax);
  }
  for (const char* input : {"#0,MW,100,AM\r", "#,MW,100,AM\r", "#4294967296,MW,100,AM\r",
                            "#1,TOO-LONG,100,AM\r", "#1,MW,100,USBB\r",
                            "#1,MW,,AM\r", "#1,MW,-1,AM\r", "#1,MW,4294967296,AM\r",
                            "#1,MW,100,AM,1234567890\r", "#1,MW,100,AM,a\tb\r"}) {
    assert(parseAdhoc(bytes(input), command) == ParseResult::InvalidArgument);
  }
  for (char byte : {char(0), char(0x7F), char(0x80)}) {
    assert(parseAdhoc(bytes(std::string("#1,MW,100,AM,") + byte + "\r"), command) == ParseResult::InvalidArgument);
  }
  assert(parseAdhoc(bytes(std::string("#1,M") + char(0) + "W,100,AM\r"), command) == ParseResult::InvalidArgument);
}

static void testStreamingLines() {
  FramerState state;
  ByteSpan frame;
  AdhocCommand command;

  for (uint8_t byte : std::string("#1,MW,100,AM"))
    assert(feed(state, adhocFraming, byte, frame) == FrameEvent::None);
  assert(feed(state, adhocFraming, '\r', frame) == FrameEvent::Ready);
  assert(parseAdhoc(frame, command) == ParseResult::Ready);
  assert(command.id == AdhocId::SetMemory);
  assert(feed(state, adhocFraming, '\n', frame) == FrameEvent::None);

  for (uint8_t byte : std::string("F7100123"))
    assert(feed(state, adhocFraming, byte, frame) == FrameEvent::None);
  assert(feed(state, adhocFraming, '\n', frame) == FrameEvent::Ready);
  assert(parseAdhoc(frame, command) == ParseResult::Ready);
  assert(command.frequency == 7100123);
  assert(feed(state, adhocFraming, 'E', frame) == FrameEvent::Ready);
  assert(parseAdhoc(frame, command) == ParseResult::Ready);
  assert(command.id == AdhocId::ShortPress);
}

static void testOverflowRecovery() {
  FramerState state;
  ByteSpan frame;
  AdhocCommand command;

  for (uint8_t prefix : {'#', 'F'}) {
    assert(feed(state, adhocFraming, prefix, frame) == FrameEvent::None);
    for (size_t i = 1; i < FramerState::BUFFER_SIZE - 1; ++i)
      assert(feed(state, adhocFraming, '1', frame) == FrameEvent::None);
    assert(feed(state, adhocFraming, '1', frame) == FrameEvent::FrameTooLong);
    // The tail of a bad line must not become single-byte commands.
    assert(feed(state, adhocFraming, 'R', frame) == FrameEvent::None);
    assert(feed(state, adhocFraming, 'O', frame) == FrameEvent::None);
    assert(feed(state, adhocFraming, '\r', frame) == FrameEvent::DiscardComplete);
    assert(feed(state, adhocFraming, '\n', frame) == FrameEvent::None);
    assert(feed(state, adhocFraming, 'R', frame) == FrameEvent::Ready);
    assert(parseAdhoc(frame, command) == ParseResult::Ready);
    assert(command.id == AdhocId::RotateCw);
  }
}

static void testTheme() {
  static_assert(1 + AdhocCommand::THEME_WORDS * 5 <= FramerState::BUFFER_SIZE,
                "A theme must fit in the frame buffer");
  std::string theme = "^";
  for (size_t i = 0; i < AdhocCommand::THEME_WORDS; ++i) theme += "x1a2B";
  theme.replace(theme.size() - 4, 4, "FFFF");
  FramerState state;
  ByteSpan frame;
  AdhocCommand command;

  for (size_t i = 0; i < theme.size() - 1; ++i)
    assert(feed(state, adhocFraming, theme[i], frame) == FrameEvent::None);
  // A theme completes at its fixed length, without a newline.
  assert(feed(state, adhocFraming, theme.back(), frame) == FrameEvent::Ready);
  assert(parseAdhoc(frame, command) == ParseResult::Ready);
  assert(command.id == AdhocId::SetTheme);
  for (size_t i = 0; i < AdhocCommand::THEME_WORDS - 1; ++i)
    assert(command.theme_words[i] == 0x1A2B);
  assert(command.theme_words[AdhocCommand::THEME_WORDS - 1] == 0xFFFF);

  // Decoded data owns its storage when the framer is reused.
  const AdhocCommand saved = command;
  assert(feed(state, adhocFraming, '@', frame) == FrameEvent::Ready);
  assert(parseAdhoc(frame, command) == ParseResult::Ready);
  assert(command.id == AdhocId::GetTheme);
  assert(saved.theme_words[0] == 0x1A2B);

  assert(parseAdhoc(bytes(theme.substr(0, theme.size() - 1)), command) == ParseResult::InvalidSyntax);
  assert(parseAdhoc(bytes(theme + "\n"), command) == ParseResult::InvalidSyntax);
  theme[1] = 'y';
  assert(parseAdhoc(bytes(theme), command) == ParseResult::InvalidSyntax);
  theme[1] = 'x';
  theme[2] = 'g';
  assert(parseAdhoc(bytes(theme), command) == ParseResult::InvalidArgument);
}

static void testTruncatedThemeRecovery() {
  FramerState state;
  ByteSpan frame;
  AdhocCommand command;

  for (uint8_t ending : {'\r', '\n'}) {
    for (uint8_t byte : std::string("^x1234"))
      assert(feed(state, adhocFraming, byte, frame) == FrameEvent::None);
    assert(feed(state, adhocFraming, ending, frame) == FrameEvent::Ready);
    assert(parseAdhoc(frame, command) == ParseResult::InvalidSyntax);
    assert(state.length == 0 && !state.discarding);
    // A trailing LF from CRLF is ignored; the next command is still recognized.
    assert(feed(state, adhocFraming, '\n', frame) == FrameEvent::None);
    assert(feed(state, adhocFraming, 'R', frame) == FrameEvent::Ready);
    assert(parseAdhoc(frame, command) == ParseResult::Ready);
    assert(command.id == AdhocId::RotateCw);
  }
}

int main() {
  testSingleByteCommands();
  testFrequency();
  testMemory();
  testInvalidMemory();
  testStreamingLines();
  testOverflowRecovery();
  testTheme();
  testTruncatedThemeRecovery();
}
