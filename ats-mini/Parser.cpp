#include "Parser.h"

using command_parser::FramingDecision;

void reset(FramerState& state) {
  state.length = 0;
  state.discarding = false;
  state.ignoring = false;
}

void ignoreCurrentFrame(FramerState& state) {
  state.ignoring = true;
}

void discardFrame(FramerState& state) {
  state.length = 0;
  state.discarding = true;
  // Preserve suppression through overflow or timeout recovery.
}

FrameEvent feed(FramerState& state, const FramingRules& framing, uint8_t byte, ByteSpan& frame) {
  frame = {};
  if (state.discarding) {
    if (!framing.isTerminator || !framing.isTerminator(byte)) return FrameEvent::None;
    reset(state);
    return FrameEvent::DiscardComplete;
  }

  state.buffer[state.length++] = byte;
  const ByteSpan input{state.buffer, state.length};
  const auto decision = framing.inspect(input);
  switch (decision) {
    case FramingDecision::Incomplete:
      if (state.length == FramerState::BUFFER_SIZE) {
        discardFrame(state);
        return state.ignoring ? FrameEvent::None : FrameEvent::FrameTooLong;
      }
      return FrameEvent::None;
    case FramingDecision::RestartAtCurrentByte:
      state.buffer[0] = byte;
      state.length = 1;
      return FrameEvent::None;
    case FramingDecision::Ignore:
      reset(state);
      return FrameEvent::None;
    case FramingDecision::BlankFrame:
    case FramingDecision::Complete: {
      const bool ignoring = state.ignoring;
      if (!ignoring && decision == FramingDecision::Complete) frame = input;
      reset(state);
      if (ignoring) return FrameEvent::DiscardComplete;
      return decision == FramingDecision::Complete ? FrameEvent::Ready : FrameEvent::BlankFrame;
    }
  }
  return FrameEvent::None;
}
