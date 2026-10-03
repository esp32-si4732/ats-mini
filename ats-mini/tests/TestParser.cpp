#include "../Parser.h"
#include "../ParserUtils.h"
#include <cassert>
#include <cstring>

using command_parser::FramingDecision;
using command_parser::lineFeed;

// For these tests, a frame is a line ending in '\n'.
static FramingDecision inspectLine(ByteSpan input) {
  return command_parser::frameLine(input, lineFeed, FramingDecision::BlankFrame);
}

static const FramingRules lines{inspectLine, lineFeed};

static void testCompleteLine() {
  FramerState state;
  ByteSpan frame;

  assert(feed(state, lines, 'h', frame) == FrameEvent::None);
  assert(feed(state, lines, 'i', frame) == FrameEvent::None);
  assert(feed(state, lines, '\n', frame) == FrameEvent::Ready);
  assert(frame.size == 3);
  assert(std::memcmp(frame.data, "hi\n", 3) == 0);
  assert(frame.data == state.buffer);
  assert(state.length == 0);

  // Starting another line clears the previous output.
  assert(feed(state, lines, 'x', frame) == FrameEvent::None);
  assert(frame.data == nullptr && frame.size == 0);
  assert(feed(state, lines, '\n', frame) == FrameEvent::Ready);
  assert(frame.size == 2);
  assert(std::memcmp(frame.data, "x\n", 2) == 0);
}

static void testBlankLine() {
  FramerState state;
  ByteSpan frame;

  assert(feed(state, lines, '\n', frame) == FrameEvent::BlankFrame);
  assert(frame.size == 0);
  assert(state.length == 0);

  assert(feed(state, lines, ' ', frame) == FrameEvent::None);
  assert(feed(state, lines, '\t', frame) == FrameEvent::None);
  assert(feed(state, lines, '\r', frame) == FrameEvent::None);
  assert(feed(state, lines, '\n', frame) == FrameEvent::BlankFrame);
  assert(frame.size == 0);
  assert(state.length == 0);
}

static void testFullBuffer() {
  FramerState state;
  ByteSpan frame;

  // The newline may occupy the last byte in the buffer.
  for (size_t i = 0; i < FramerState::BUFFER_SIZE - 1; ++i)
    assert(feed(state, lines, 'x', frame) == FrameEvent::None);
  assert(feed(state, lines, '\n', frame) == FrameEvent::Ready);
  assert(frame.size == FramerState::BUFFER_SIZE);

  // Without a newline, a full buffer starts discarding input.
  for (size_t i = 0; i < FramerState::BUFFER_SIZE - 1; ++i)
    assert(feed(state, lines, 'x', frame) == FrameEvent::None);
  assert(feed(state, lines, 'x', frame) == FrameEvent::FrameTooLong);
  assert(state.discarding);
  assert(frame.size == 0);
  assert(feed(state, lines, 'x', frame) == FrameEvent::None);
  assert(feed(state, lines, '\n', frame) == FrameEvent::DiscardComplete);
  assert(!state.discarding);
  assert(feed(state, lines, '\n', frame) == FrameEvent::BlankFrame);
}

static void testDiscardAndReset() {
  FramerState state;
  ByteSpan frame;

  assert(feed(state, lines, 'a', frame) == FrameEvent::None);
  discardFrame(state);
  assert(state.length == 0 && state.discarding);
  assert(feed(state, lines, 'b', frame) == FrameEvent::None);
  assert(feed(state, lines, '\n', frame) == FrameEvent::DiscardComplete);

  // Reset forgets a partial line immediately.
  assert(feed(state, lines, 'c', frame) == FrameEvent::None);
  reset(state);
  assert(feed(state, lines, '\n', frame) == FrameEvent::BlankFrame);

  // It also cancels an ongoing discard.
  discardFrame(state);
  reset(state);
  assert(!state.discarding);
  assert(feed(state, lines, '\n', frame) == FrameEvent::BlankFrame);
}

static void testIgnoreAndRestart() {
  // A test rule: '#' drops the buffered input; '^' starts a fresh frame.
  const FramingRules rules{
    [](ByteSpan input) {
      const uint8_t last = input.data[input.size - 1];
      if (last == '#') return FramingDecision::Ignore;
      if (last == '^') return FramingDecision::RestartAtCurrentByte;
      return inspectLine(input);
    },
    lineFeed
  };
  FramerState state;
  ByteSpan frame;

  assert(feed(state, rules, 'a', frame) == FrameEvent::None);
  assert(feed(state, rules, '#', frame) == FrameEvent::None);
  assert(state.length == 0);
  assert(feed(state, rules, 'b', frame) == FrameEvent::None);
  assert(feed(state, rules, '^', frame) == FrameEvent::None);
  assert(feed(state, rules, '\n', frame) == FrameEvent::Ready);
  assert(frame.size == 2);
  assert(std::memcmp(frame.data, "^\n", 2) == 0);
}

static void testDiscardWithoutTerminator() {
  const FramingRules rules{inspectLine, nullptr};
  FramerState state;
  ByteSpan frame;

  // Without a recovery delimiter, only reset can end a discard.
  discardFrame(state);
  assert(feed(state, rules, '\n', frame) == FrameEvent::None);
  assert(state.discarding);
  reset(state);
  assert(feed(state, rules, '\n', frame) == FrameEvent::BlankFrame);
}

static void testIgnoredFrames() {
  FramerState state;
  ByteSpan frame;

  // Suppress a partial line, then deliver the following command normally.
  assert(feed(state, lines, 'a', frame) == FrameEvent::None);
  ignoreCurrentFrame(state);
  assert(feed(state, lines, '\n', frame) == FrameEvent::DiscardComplete);
  assert(!state.ignoring && frame.size == 0);
  assert(feed(state, lines, 'b', frame) == FrameEvent::None);
  assert(feed(state, lines, '\n', frame) == FrameEvent::Ready);

  // Suppression also works for blank and fixed-length frames without a delimiter.
  ignoreCurrentFrame(state);
  assert(feed(state, lines, '\n', frame) == FrameEvent::DiscardComplete);
  const FramingRules pair{
    [](ByteSpan input) {
      return input.size == 2 ? FramingDecision::Complete : FramingDecision::Incomplete;
    }, nullptr
  };
  ignoreCurrentFrame(state);
  assert(feed(state, pair, 'x', frame) == FrameEvent::None);
  assert(feed(state, pair, 'y', frame) == FrameEvent::DiscardComplete);
  assert(!state.ignoring && frame.size == 0);

  // Overflow stays silent and preserves suppression until recovery completes.
  ignoreCurrentFrame(state);
  for (size_t i = 0; i < FramerState::BUFFER_SIZE; ++i)
    assert(feed(state, lines, 'x', frame) == FrameEvent::None);
  assert(state.ignoring && state.discarding);
  assert(feed(state, lines, '\n', frame) == FrameEvent::DiscardComplete);
  assert(!state.ignoring && !state.discarding);

  // Timeout recovery preserves suppression; an explicit reset cancels it.
  ignoreCurrentFrame(state);
  discardFrame(state);
  assert(state.ignoring);
  reset(state);
  assert(!state.ignoring && !state.discarding);
}

static void testSuppressionTransitions() {
  const FramingRules rules{
    [](ByteSpan input) {
      const uint8_t last = input.data[input.size - 1];
      if (last == '#') return FramingDecision::Ignore;
      if (last == '^') return FramingDecision::RestartAtCurrentByte;
      return inspectLine(input);
    }, lineFeed
  };
  FramerState state;
  ByteSpan frame;

  // Ignored input cancels suppression, even when requested while idle.
  ignoreCurrentFrame(state);
  assert(feed(state, rules, '#', frame) == FrameEvent::None);
  assert(!state.ignoring && state.length == 0 && frame.data == nullptr);
  assert(feed(state, rules, 'a', frame) == FrameEvent::None);
  assert(feed(state, rules, '\n', frame) == FrameEvent::Ready);
  assert(frame.size == 2 && std::memcmp(frame.data, "a\n", 2) == 0);

  // Restarting framing preserves suppression until the new frame completes.
  assert(feed(state, rules, 'b', frame) == FrameEvent::None);
  ignoreCurrentFrame(state);
  assert(feed(state, rules, '^', frame) == FrameEvent::None);
  assert(state.ignoring && state.length == 1 && state.buffer[0] == '^');
  assert(feed(state, rules, '\n', frame) == FrameEvent::DiscardComplete);
  assert(!state.ignoring && state.length == 0);
  assert(frame.data == nullptr && frame.size == 0);
  assert(feed(state, rules, 'c', frame) == FrameEvent::None);
  assert(feed(state, rules, '\n', frame) == FrameEvent::Ready);
  assert(frame.size == 2 && std::memcmp(frame.data, "c\n", 2) == 0);
}

int main() {
  testSuppressionTransitions();
  testIgnoredFrames();
  testCompleteLine();
  testBlankLine();
  testFullBuffer();
  testDiscardAndReset();
  testIgnoreAndRestart();
  testDiscardWithoutTerminator();
}
