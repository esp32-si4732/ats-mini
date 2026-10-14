#include "Common.h"
#include "Themes.h"
#include "Utils.h"
#include "Menu.h"
#include "Memories.h"
#include "Draw.h"
#include "Storage.h"
#include "Remote.h"
#include "ParserAdhoc.h"

static RemoteState remoteSerialState;
static bool serialDisconnectPending = false;
static uint32_t serialDisconnectedAt;
static constexpr uint32_t REMOTE_INPUT_TIMEOUT_MS = 30000;

static_assert(1 + AdhocCommand::THEME_WORDS * 5 <= FramerState::BUFFER_SIZE,
              "A theme must fit in the remote input buffer");

static bool remoteWriteScreenshot(Stream* stream, const uint8_t* data, size_t size)
{
  if(stream != &Serial) return stream->write(data, size) == size;

  // HWCDC can discard queued bytes on a false disconnect. Only enqueue data
  // that fits; screenshot output has no competing Serial writer.
  uint32_t lastProgress = millis();
  while(size)
  {
    if(millis() - lastProgress >= stream->getTimeout()) return false;
    int space = Serial.availableForWrite();
    if(space <= 0)
    {
      yield();
      continue;
    }
    size_t chunk = size < (size_t)space ? size : (size_t)space;
    size_t written = Serial.write(data, chunk);
    if(!written) return false;
    lastProgress = millis();
    data += written;
    size -= written;
  }
  return true;
}

// BMP dimensions match the display sprite; all fields are little-endian.
static const uint8_t bmpHeader[] = {
  'B', 'M',
  (uint8_t)BMP_FILE_SIZE, (uint8_t)(BMP_FILE_SIZE >> 8),
  (uint8_t)(BMP_FILE_SIZE >> 16), (uint8_t)(BMP_FILE_SIZE >> 24),
  0, 0, 0, 0, 66, 0, 0, 0,
  40, 0, 0, 0,                         // BITMAPINFOHEADER size
  (uint8_t)DISPLAY_WIDTH, (uint8_t)(DISPLAY_WIDTH >> 8), 0, 0,
  (uint8_t)DISPLAY_HEIGHT, (uint8_t)(DISPLAY_HEIGHT >> 8), 0, 0,
  1, 0, 16, 0, 3, 0, 0, 0,            // One plane, RGB565 bitfields
  0, 0, 0, 0, 0, 0, 0, 0,             // Image size and X resolution
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Y resolution and palette
  0, 0xF8, 0, 0, 0xE0, 0x07, 0, 0, 0x1F, 0, 0, 0 // RGB masks
};

//
// Send the screen as a hex ('C') or binary ('c') RGB565 BMP.
//
template<typename Write>
static bool captureScreen(Write write, bool binary)
{
  const uint16_t *fb = (const uint16_t *)spr.getBuffer();
  uint8_t buf[256];
  if(!fb) return false;

  // Keep chunks full across the header and rows. All output is added in pairs.
  uint8_t *p = buf;
  auto appendPair = [&](uint8_t first, uint8_t second) {
    *p++ = first;
    *p++ = second;
    if(p == buf + sizeof(buf))
    {
      if(!write(buf, sizeof(buf))) return false;
      p = buf;
    }
    return true;
  };
  auto appendHexByte = [&](uint8_t byte) {
    static const char hex[] = "0123456789abcdef";
    return appendPair(hex[byte >> 4], hex[byte & 0xF]);
  };
  auto appendBmpPair = [&](uint8_t first, uint8_t second) {
    return binary ? appendPair(first, second)
                  : appendHexByte(first) && appendHexByte(second);
  };

  if(!binary && !appendPair('\r', '\n')) return false;
  for(size_t i=0 ; i<sizeof(bmpHeader) ; i+=2)
  {
    if(!appendBmpPair(bmpHeader[i], bmpHeader[i + 1])) return false;
  }
  if(!binary && !appendPair('\r', '\n')) return false;

  // Send rows bottom-up. The unrotated sprite stores contiguous rows of
  // byte-swapped RGB565 words; emit the high byte first for BMP pixel data.
  for(int y=DISPLAY_HEIGHT-1 ; y>=0 ; y--)
  {
    const uint16_t *row = fb + (uint32_t)y * DISPLAY_WIDTH;
    for(int x=0 ; x<DISPLAY_WIDTH ; x++)
    {
      uint16_t v = row[x];
      if(!appendBmpPair(v >> 8, v & 0xFF)) return false;
    }
    if(!binary && !appendPair('\r', '\n')) return false;
  }
  if(p != buf && !write(buf, p - buf)) return false;
  return true;
}

static void remoteCaptureScreen(Stream* stream, bool binary)
{
  if(!captureScreen([&](const uint8_t *data, size_t size) {
    return remoteWriteScreenshot(stream, data, size);
  }, binary)) return;
  // HWCDC flush() can discard pending output on a false disconnect.
  if(stream != &Serial) stream->flush();
}

bool remoteCaptureBmp(uint8_t *bmp)
{
  return captureScreen([&](const uint8_t *data, size_t length) {
    memcpy(bmp, data, length);
    bmp += length;
    return true;
  }, true);
}

static bool remoteShowError(Stream* stream, const char *message)
{
  stream->printf("\r\nError: %s\r\n", message);
  return false;
}

static bool remoteSetFrequency(Stream *stream, uint32_t freqHz)
{
  if(!updateFrequency(freqHz, false))
    return remoteShowError(stream, "Frequency is out of range for the current band");

  clearStationInfo();
  identifyFrequency(currentFrequency);

  return true;
}

static void remoteGetMemories(Stream* stream)
{
  for (uint8_t i = 0; i < getTotalMemories(); i++) {
    const Memory memory = getMemory(i);
    if (memory.freq) {
      stream->printf("#%02d,%s,%lu,%s%s%s\r\n", i + 1, bands[memory.band].bandName,
                     (unsigned long)memory.freq, bandModeDesc[memory.mode],
                     memory.name[0] ? "," : "", memory.name);
    }
  }
}

static bool remoteSetMemory(Stream* stream, const AdhocCommand& command)
{
  const uint32_t slot = command.memory_slot;
  if(slot < 1 || slot > (uint32_t)getTotalMemories())
    return remoteShowError(stream, "Invalid memory slot number");

  Memory mem = getMemory(slot - 1);
  mem.mode = 15;
  for (int i = 0; i < getTotalModes(); i++) {
    if (strcmp(bandModeDesc[i], command.mode) == 0) {
      mem.mode = i;
      break;
    }
  }
  if (mem.mode == 15)
    return remoteShowError(stream, "No such mode");

  mem.freq = command.frequency;
  mem.band = 0xFF;
  bool knownBand = false;
  for (int i = 0; i < getTotalBands(); i++) {
    if (strcmp(bands[i].bandName, command.band) != 0) continue;
    knownBand = true;
    // Match frequency as well as name for duplicate bands (15M).
    // A zero frequency clears the slot, so only its band name must match.
    if (!mem.freq || isMemoryInBand(&bands[i], &mem)) {
      mem.band = i;
      break;
    }
  }
  if (mem.band == 0xFF)
    return remoteShowError(stream, knownBand ? "Invalid frequency or mode" : "No such band");

  if(command.has_name)
    memcpy(mem.name, command.name, sizeof(mem.name));
  setMemory(slot-1, mem);
  return true;
}

//
// Set current color theme from the remote
//
static void remoteSetColorTheme(Stream* stream, const AdhocCommand& command)
{
  memcpy((uint8_t *)&TH + offsetof(ColorTheme, bg), command.theme_words, sizeof(command.theme_words));
  stream->println(" Ok");
  drawScreen();
}

//
// Print current color theme to the remote
//
static void remoteGetColorTheme(Stream* stream)
{
  stream->printf("Color theme %s: ", TH.name);
  const uint8_t *p = (uint8_t *)&(TH.bg);

  for(size_t i=0 ; i<sizeof(ColorTheme)-offsetof(ColorTheme, bg) ; i+=sizeof(uint16_t))
  {
    stream->printf("x%02X%02X", p[i+1], p[i]);
  }

  stream->println();
}

//
// Print current status to the remote
//
static void remotePrintStatus(Stream* stream, RemoteState* state)
{
  // Prepare information ready to be sent
  float remoteVoltage = batteryMonitor();

  // S-Meter conditional on compile option
  rx.getCurrentReceivedSignalQuality();
  uint8_t remoteRssi = rx.getCurrentRSSI();
  uint8_t remoteSnr = rx.getCurrentSNR();

  // Use rx.getFrequency to force read of capacitor value from SI4732/5
  rx.getFrequency();
  uint16_t tuningCapacitor = rx.getAntennaTuningCapacitor();

  // Remote serial
  stream->printf("%u,%u,%d,%d,%s,%s,%s,%s,%hu,%hu,%hu,%hu,%hu,%.2f,%hu\r\n",
                VER_APP,
                getReceiverFrequency(currentFrequency),
                getCurrentBFO(),
                ((currentMode == USB) ? getCurrentBand()->usbCal :
                 (currentMode == LSB) ? getCurrentBand()->lsbCal : 0),
                getCurrentBand()->bandName,
                bandModeDesc[currentMode],
                getCurrentStep()->desc,
                getCurrentBandwidth()->desc,
                agcIdx,
                volume,
                remoteRssi,
                remoteSnr,
                tuningCapacitor,
                remoteVoltage,
                state->remoteSeqnum
                );
}

//
// Execute a validated remote command
//
static bool remoteExecuteCommand(Stream* stream, RemoteState* state, const AdhocCommand& command)
{
  using Id = AdhocCommand::Id;
  switch(command.id)
  {
    case Id::RotateCw:   return handleRotation(1, 1);
    case Id::RotateCcw:  return handleRotation(-1, -1);
    case Id::Click:      return handleClick(false);
    case Id::ShortPress: return handleClick(true);
    case Id::BandUp:      doBand(1); break;
    case Id::BandDown:    doBand(-1); break;
    case Id::ModeUp:      doMode(1); break;
    case Id::ModeDown:    doMode(-1); break;
    case Id::StepUp:      doStep(1); break;
    case Id::StepDown:    doStep(-1); break;
    case Id::BandwidthUp:   doBandwidth(1); break;
    case Id::BandwidthDown: doBandwidth(-1); break;
    case Id::AgcUp:       doAgc(1); break;
    case Id::AgcDown:     doAgc(-1); break;
    case Id::VolumeUp:    doVolume(1); break;
    case Id::VolumeDown:  doVolume(-1); break;
    case Id::BacklightUp:   doBrt(1); break;
    case Id::BacklightDown: doBrt(-1); break;
    case Id::CalUp:       doCal(1); break;
    case Id::CalDown:     doCal(-1); break;
    case Id::SleepOn:     sleepOn(true); return true;
    case Id::SleepOff:    sleepOn(false); return true;
    case Id::SetMemory:
      if(!remoteSetMemory(stream, command)) return false;
      prefsRequestSave(SAVE_MEMORIES);
      break;
    case Id::SetFrequency:
      if(!remoteSetFrequency(stream, command.frequency)) return false;
      prefsRequestSave(SAVE_CUR_BAND);
      break;
    case Id::CaptureScreen:
    case Id::CaptureScreenBinary:
      state->remoteLogOn = false;
      remoteCaptureScreen(stream, command.id == Id::CaptureScreenBinary);
      return false;
    case Id::ToggleLog:
      state->remoteLogOn = !state->remoteLogOn;
      return false;
    case Id::GetMemories:
      remoteGetMemories(stream);
      return false;
    case Id::ToggleThemeEditor:
      stream->println(switchThemeEditor(!switchThemeEditor()) ? "Theme editor enabled" : "Theme editor disabled");
      return true;
    case Id::SetTheme:
      if(switchThemeEditor()) remoteSetColorTheme(stream, command);
      return false;
    case Id::GetTheme:
      if(switchThemeEditor()) remoteGetColorTheme(stream);
      return false;
    default:
      return false;
  }
  return true;
}

static void remoteTickSession(Stream* stream, RemoteState* state)
{
  const uint32_t now = millis();
  if(state->input.length && now - state->inputTimer >= REMOTE_INPUT_TIMEOUT_MS)
  {
    const bool report = !state->input.ignoring &&
      (state->input.buffer[0] != '^' || switchThemeEditor());
    discardFrame(state->input);
    if(report) remoteShowError(stream, "Command timed out; send CR or LF to resume");
  }
  if(state->input.length || state->input.discarding || !state->remoteLogOn) return;
  if(now - state->remoteTimer >= 500)
  {
    state->remoteTimer = now;
    state->remoteSeqnum++;
    remotePrintStatus(stream, state);
  }
}

// Echo and prompts are ad hoc terminal behavior, not framing rules.
static void remoteEchoInput(Stream* stream, const FramerState& input, uint8_t byte)
{
  if(input.ignoring || input.discarding) return;
  const uint8_t first = input.length ? input.buffer[0] : byte;
  if(!input.length && byte == '^')
  {
    if(switchThemeEditor())
      stream->print("Enter a string of hex colors (x0001x0002...): ");
  }
  else if(first == '#' || first == 'F' || (first == '^' && switchThemeEditor()))
  {
    if(byte != '\r' && byte != '\n') stream->write(byte);
  }
}

static bool remoteHandleFrame(Stream* stream, RemoteState* state, ByteSpan frame)
{
  if(frame.data[0] == '^' && !switchThemeEditor()) return false;
  AdhocCommand command;
  const ParseResult result = parseAdhoc(frame, command);
  if(result != ParseResult::Ready)
  {
    if(frame.data[0] == '^') stream->println(" Err");
    else if(result == ParseResult::InvalidSyntax)
      remoteShowError(stream, "Invalid command syntax");
    else if(result == ParseResult::InvalidArgument)
      remoteShowError(stream, "Invalid command argument");
    return false;
  }
  if(command.id == AdhocCommand::Id::SetMemory || command.id == AdhocCommand::Id::SetFrequency)
    stream->println();
  return remoteExecuteCommand(stream, state, command);
}

// Normal polling and seek cancellation share framing, timestamps and resets.
// A null output means cancellation: skip ignored input, then suppress a command.
static bool remotePoll(Stream* output, RemoteState* state, RemoteReadInput readInput)
{
  for(size_t i = 0; i < FramerState::BUFFER_SIZE; ++i)
  {
    uint8_t byte;
    const InputEvent input = readInput(byte);
    if(input == InputEvent::SessionReset)
    {
      *state = RemoteState{};
      return false;
    }
    // Observe connection changes before timeout or periodic output.
    if(output && i == 0) remoteTickSession(output, state);
    if(input == InputEvent::NoData) return false;

    if(output) remoteEchoInput(output, state->input, byte);
    else ignoreCurrentFrame(state->input);

    ByteSpan frame;
    const FrameEvent event = feed(state->input, command_parser::adhocFraming, byte, frame);
    state->inputTimer = millis();
    if(!output)
    {
      // An ignored separator leaves no frame or discard in progress.
      if(event == FrameEvent::None && !state->input.length && !state->input.discarding)
        continue;
      return true;
    }

    switch(event)
    {
      case FrameEvent::None: continue;
      case FrameEvent::Ready: return remoteHandleFrame(output, state, frame);
      case FrameEvent::FrameTooLong:
        remoteShowError(output, "Command is too long");
        return false;
      default: return false;
    }
  }
  return false;
}

bool remoteLoop(Stream* stream, RemoteState* state, RemoteReadInput readInput)
{
  return remotePoll(stream, state, readInput);
}

bool remoteConsumeAbortPending(RemoteState* state, RemoteReadInput readInput)
{
  return remotePoll(nullptr, state, readInput);
}

static InputEvent serialReadInput(uint8_t& byte)
{
  // HWCDC can briefly report a false disconnect. Queued input takes priority;
  // only a sustained disconnect without input should erase a partial command.
  if(!Serial && !Serial.available())
  {
    if(!serialDisconnectPending)
    {
      serialDisconnectedAt = millis();
      serialDisconnectPending = true;
    }
    return millis() - serialDisconnectedAt >= 1000
      ? InputEvent::SessionReset : InputEvent::NoData;
  }
  serialDisconnectPending = false;
  int value = Serial.read();
  if(value < 0) return InputEvent::NoData;
  byte = uint8_t(value);
  return InputEvent::Byte;
}

bool serialLoop(uint8_t usbMode)
{
  if(usbMode == USB_OFF)
  {
    remoteSerialState = RemoteState{};
    serialDisconnectPending = false;
    return false;
  }
  return remoteLoop(&Serial, &remoteSerialState, serialReadInput);
}

bool serialConsumeAbortPending(uint8_t usbMode)
{
  return usbMode != USB_OFF && remoteConsumeAbortPending(&remoteSerialState, serialReadInput);
}
