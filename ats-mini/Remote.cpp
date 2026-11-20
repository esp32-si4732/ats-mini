#include "Remote.h"
#include "ParserAdhoc.h"
#include "ParserKenwood.h"

static constexpr uint32_t REMOTE_INPUT_TIMEOUT_MS = 30000;

static void remoteTickSession(Stream& stream, RemoteState& state, RemoteProtocol protocol)
{
  const uint32_t now = millis();
  if(state.input.length && now - state.inputTimer >= REMOTE_INPUT_TIMEOUT_MS)
  {
    if(protocol == RemoteProtocol::Kenwood)
    {
      if(!state.input.ignoring) remoteKenwoodError(stream);
    }
    else remoteAdhocTimeout(stream, state.input);
    discardFrame(state.input);
  }
  if(state.input.length || state.input.discarding) return;
  if(protocol == RemoteProtocol::Kenwood) remoteKenwoodTick(stream, state.kenwood);
  else remoteAdhocTick(stream, state.adhoc);
}

// Normal polling and seek cancellation share framing, timestamps and resets.
// A null output means cancellation: skip ignored input, then suppress a command.
static bool remotePoll(Stream* output, RemoteState& state, RemoteReadInput readInput, RemoteProtocol protocol)
{
  const FramingRules& framing = protocol == RemoteProtocol::Kenwood
    ? command_parser::kenwoodFraming : command_parser::adhocFraming;
  for(size_t i = 0; i < FramerState::BUFFER_SIZE; ++i)
  {
    uint8_t byte;
    const InputEvent input = readInput(byte);
    if(input == InputEvent::SessionReset)
    {
      state = RemoteState{};
      return false;
    }
    // Observe connection changes before timeout or periodic output.
    if(output && i == 0) remoteTickSession(*output, state, protocol);
    if(input == InputEvent::NoData) return false;

    if(!output) ignoreCurrentFrame(state.input);
    else if(protocol == RemoteProtocol::Adhoc) remoteAdhocEcho(*output, state.input, byte);

    ByteSpan frame;
    const FrameEvent event = feed(state.input, framing, byte, frame);
    state.inputTimer = millis();
    if(!output)
    {
      // An ignored separator leaves no frame or discard in progress.
      if(event == FrameEvent::None && !state.input.length && !state.input.discarding)
        continue;
      return true;
    }

    switch(event)
    {
      case FrameEvent::None: continue;
      case FrameEvent::Ready:
        if(protocol == RemoteProtocol::Kenwood) return remoteKenwoodFrame(*output, state.kenwood, frame);
        return remoteAdhocFrame(*output, state.adhoc, frame);
      case FrameEvent::FrameTooLong:
        if(protocol == RemoteProtocol::Kenwood) remoteKenwoodError(*output);
        else remoteAdhocOverflow(*output);
        return false;
      default: return false;
    }
  }
  return false;
}

bool remoteLoop(Stream& stream, RemoteState& state, RemoteReadInput readInput, RemoteProtocol protocol)
{
  return remotePoll(&stream, state, readInput, protocol);
}

bool remoteConsumeAbortPending(RemoteState& state, RemoteReadInput readInput, RemoteProtocol protocol)
{
  return remotePoll(nullptr, state, readInput, protocol);
}
