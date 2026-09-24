#include "Common.h"
#include "Themes.h"
#include "Utils.h"
#include "Menu.h"
#include "Draw.h"
#include "Remote.h"

static RemoteState remoteSerialState;

static uint8_t char2nibble(char key)
{
  if((key >= '0') && (key <= '9')) return(key - '0');
  if((key >= 'A') && (key <= 'F')) return(key - 'A' + 10);
  if((key >= 'a') && (key <= 'f')) return(key - 'a' + 10);
  return(0);
}

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

static constexpr uint32_t BMP_FILE_SIZE = 66 + (uint32_t)DISPLAY_WIDTH * DISPLAY_HEIGHT * 2;

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
static void remoteCaptureScreen(Stream* stream, bool binary)
{
  uint16_t width  = spr.width();
  uint16_t height = spr.height();
  const uint16_t *fb = (const uint16_t *)spr.getBuffer();
  uint8_t buf[256];
  if(!fb) return;

  // Keep chunks full across the header and rows. All output is added in pairs.
  uint8_t *p = buf;
  auto appendPair = [&](uint8_t first, uint8_t second) {
    *p++ = first;
    *p++ = second;
    if(p == buf + sizeof(buf))
    {
      if(!remoteWriteScreenshot(stream, buf, sizeof(buf))) return false;
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

  if(!binary && !appendPair('\r', '\n')) return;
  for(size_t i=0 ; i<sizeof(bmpHeader) ; i+=2)
  {
    if(!appendBmpPair(bmpHeader[i], bmpHeader[i + 1])) return;
  }
  if(!binary && !appendPair('\r', '\n')) return;

  // Send rows bottom-up. The unrotated sprite stores contiguous rows of
  // byte-swapped RGB565 words; emit the high byte first for BMP pixel data.
  for(int y=height-1 ; y>=0 ; y--)
  {
    const uint16_t *row = fb + (uint32_t)y * width;
    for(int x=0 ; x<width ; x++)
    {
      uint16_t v = row[x];
      if(!appendBmpPair(v >> 8, v & 0xFF)) return;
    }
    if(!binary && !appendPair('\r', '\n')) return;
  }
  if(p != buf && !remoteWriteScreenshot(stream, buf, p - buf)) return;
  // HWCDC flush() can discard pending output on a false disconnect.
  if(stream != &Serial) stream->flush();
}

char remoteReadChar(Stream* stream)
{
  char key;

  while (!stream->available());
  key = stream->read();
  stream->print(key);
  return key;
}

long int remoteReadInteger(Stream* stream)
{
  long int result = 0;
  while (true) {
    char ch = stream->peek();
    if (ch == 0xFF) {
      continue;
    } else if ((ch >= '0') && (ch <= '9')) {
      ch = remoteReadChar(stream);
      // Can overflow, but it's ok
      result = result * 10 + (ch - '0');
    } else {
      return result;
    }
  }
}

void remoteReadString(Stream* stream, char *bufStr, uint8_t bufLen)
{
  uint8_t length = 0;
  while (true) {
    char ch = stream->peek();
    if (ch == 0xFF) {
      continue;
    } else if (ch == ',' || ch < ' ') {
      bufStr[length] = '\0';
      return;
    } else {
      ch = remoteReadChar(stream);
      bufStr[length] = ch;
      if (++length >= bufLen - 1) {
        bufStr[length] = '\0';
        return;
      }
    }
  }
}

static bool expectNewline(Stream* stream)
{
  char ch;
  while ((ch = stream->peek()) == 0xFF);
  if (ch == '\r') {
    stream->read();
    return true;
  }
  return false;
}

static bool remoteShowError(Stream* stream, const char *message)
{
  // Consume the remaining input
  while (stream->available()) remoteReadChar(stream);
  stream->printf("\r\nError: %s\r\n", message);
  return false;
}

static bool remoteSetFrequency(Stream *stream)
{
  stream->print('F');

  long int freqHz = remoteReadInteger(stream);
  if(freqHz <= 0)
    return remoteShowError(stream, "Invalid frequency");
  if(!expectNewline(stream))
    return remoteShowError(stream, "Expected newline");
  stream->println();

  Band *band = getCurrentBand();
  uint16_t targetFreq = freqFromHz(freqHz, currentMode);
  int targetBfo = isSSB() ? bfoFromHz(freqHz) : 0;
  if(!isFreqInBand(band, targetFreq) || (isSSB() && targetFreq == band->maximumFreq && targetBfo))
    return remoteShowError(stream, "Frequency is out of range for the current band");
  if(!updateFrequency(targetFreq, false))
    return remoteShowError(stream, "Frequency is out of range for the current band");

  if(isSSB())
    updateBFO(targetBfo, false);
  else if(currentBFO)
    updateBFO(0, true);

  clearStationInfo();
  identifyFrequency(currentFrequency + currentBFO / 1000);

  return true;
}

static void remoteGetMemories(Stream* stream)
{
  for (uint8_t i = 0; i < getTotalMemories(); i++) {
    if (memories[i].freq) {
      stream->printf("#%02d,%s,%ld,%s\r\n", i + 1, bands[memories[i].band].bandName, memories[i].freq, bandModeDesc[memories[i].mode]);
    }
  }
}

static bool remoteSetMemory(Stream* stream)
{
  stream->print('#');
  Memory mem;
  uint32_t freq = 0;

  long int slot = remoteReadInteger(stream);
  if (remoteReadChar(stream) != ',')
    return remoteShowError(stream, "Expected ','");
  if (slot < 1 || slot > getTotalMemories())
    return remoteShowError(stream, "Invalid memory slot number");

  char band[8];
  remoteReadString(stream, band, 8);
  if (remoteReadChar(stream) != ',')
    return remoteShowError(stream, "Expected ','");
  mem.band = 0xFF;
  for (int i = 0; i < getTotalBands(); i++) {
    if (strcmp(bands[i].bandName, band) == 0) {
      mem.band = i;
      break;
    }
  }
  if (mem.band == 0xFF)
    return remoteShowError(stream, "No such band");

  freq = remoteReadInteger(stream);
  if (remoteReadChar(stream) != ',')
    return remoteShowError(stream, "Expected ','");

  char mode[4];
  remoteReadString(stream, mode, 4);
  if (!expectNewline(stream))
    return remoteShowError(stream, "Expected newline");
  stream->println();
  mem.mode = 15;
  for (int i = 0; i < getTotalModes(); i++) {
    if (strcmp(bandModeDesc[i], mode) == 0) {
      mem.mode = i;
      break;
    }
  }
  if (mem.mode == 15)
    return remoteShowError(stream, "No such mode");

  mem.freq = freq;

  if (!isMemoryInBand(&bands[mem.band], &mem)) {
    if (!freq) {
      // Clear slot
      memories[slot-1] = mem;
      return true;
    } else {
      // Handle duplicate band names (15M)
      mem.band = 0xFF;
      for (int i = getTotalBands()-1; i >= 0; i--) {
        if (strcmp(bands[i].bandName, band) == 0) {
          mem.band = i;
          break;
        }
      }
      if (mem.band == 0xFF)
        return remoteShowError(stream, "No such band");
      if (!isMemoryInBand(&bands[mem.band], &mem))
        return remoteShowError(stream, "Invalid frequency or mode");
    }
  }

  memories[slot-1] = mem;
  return true;
}

//
// Set current color theme from the remote
//
static void remoteSetColorTheme(Stream* stream)
{
  stream->print("Enter a string of hex colors (x0001x0002...): ");

  uint8_t *p = (uint8_t *)&(TH.bg);

  for(int i=0 ; ; i+=sizeof(uint16_t))
  {
    if(i >= sizeof(ColorTheme)-offsetof(ColorTheme, bg))
    {
      stream->println(" Ok");
      break;
    }

    if(remoteReadChar(stream) != 'x')
    {
      stream->println(" Err");
      break;
    }

    p[i + 1]  = char2nibble(remoteReadChar(stream)) * 16;
    p[i + 1] |= char2nibble(remoteReadChar(stream));
    p[i]      = char2nibble(remoteReadChar(stream)) * 16;
    p[i]     |= char2nibble(remoteReadChar(stream));
  }

  // Redraw screen
  drawScreen();
}

//
// Print current color theme to the remote
//
static void remoteGetColorTheme(Stream* stream)
{
  stream->printf("Color theme %s: ", TH.name);
  const uint8_t *p = (uint8_t *)&(TH.bg);

  for(int i=0 ; i<sizeof(ColorTheme)-offsetof(ColorTheme, bg) ; i+=sizeof(uint16_t))
  {
    stream->printf("x%02X%02X", p[i+1], p[i]);
  }

  stream->println();
}

//
// Print current status to the remote
//
void remotePrintStatus(Stream* stream, RemoteState* state)
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
                currentFrequency,
                currentBFO,
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
// Tick remote time, periodically printing status
//
void remoteTickTime(Stream* stream, RemoteState* state)
{
  if(state->remoteLogOn && (millis() - state->remoteTimer >= 500))
  {
    // Mark time and increment diagnostic sequence number
    state->remoteTimer = millis();
    state->remoteSeqnum++;
    // Show status
    remotePrintStatus(stream, state);
  }
}

//
// Recognize and execute given remote command
//
int remoteDoCommand(Stream* stream, RemoteState* state, char key)
{
  int event = 0;

  switch(key)
  {
    case 'R': // Rotate Encoder Clockwise
      event |= 1 << REMOTE_DIRECTION;
      event |= REMOTE_PREFS;
      break;
    case 'r': // Rotate Encoder Counterclockwise
      event |= -1 << REMOTE_DIRECTION;
      event |= REMOTE_PREFS;
      break;
    case 'e': // Encoder Push Button
      event |= REMOTE_CLICK;
      break;
    case 'E': // Encoder Short Press
      event |= REMOTE_SHORT_PRESS;
      break;
    case 'B': // Band Up
      doBand(1);
      event |= REMOTE_PREFS;
      break;
    case 'b': // Band Down
      doBand(-1);
      event |= REMOTE_PREFS;
      break;
    case 'M': // Mode Up
      doMode(1);
      event |= REMOTE_PREFS;
      break;
    case 'm': // Mode Down
      doMode(-1);
      event |= REMOTE_PREFS;
      break;
    case 'S': // Step Up
      doStep(1);
      event |= REMOTE_PREFS;
      break;
    case 's': // Step Down
      doStep(-1);
      event |= REMOTE_PREFS;
      break;
    case 'W': // Bandwidth Up
      doBandwidth(1);
      event |= REMOTE_PREFS;
      break;
    case 'w': // Bandwidth Down
      doBandwidth(-1);
      event |= REMOTE_PREFS;
      break;
    case 'A': // AGC/ATTN Up
      doAgc(1);
      event |= REMOTE_PREFS;
      break;
    case 'a': // AGC/ATTN Down
      doAgc(-1);
      event |= REMOTE_PREFS;
      break;
    case 'V': // Volume Up
      doVolume(1);
      event |= REMOTE_PREFS;
      break;
    case 'v': // Volume Down
      doVolume(-1);
      event |= REMOTE_PREFS;
      break;
    case 'L': // Backlight Up
      doBrt(1);
      event |= REMOTE_PREFS;
      break;
    case 'l': // Backlight Down
      doBrt(-1);
      event |= REMOTE_PREFS;
      break;
    case 'O':
      sleepOn(true);
      break;
    case 'o':
      sleepOn(false);
      break;
    case 'I':
      doCal(1);
      event |= REMOTE_PREFS;
      break;
    case 'i':
      doCal(-1);
      event |= REMOTE_PREFS;
      break;
    case 'C':
    case 'c':
      state->remoteLogOn = false;
      remoteCaptureScreen(stream, key == 'c');
      break;
    case 't':
      state->remoteLogOn = !state->remoteLogOn;
      break;

    case '$':
      remoteGetMemories(stream);
      break;
    case '#':
      if (remoteSetMemory(stream))
        event |= REMOTE_PREFS;
      break;
    case 'F':
      if (remoteSetFrequency(stream))
        event |= REMOTE_PREFS;
      break;

    case 'T':
      stream->println(switchThemeEditor(!switchThemeEditor()) ? "Theme editor enabled" : "Theme editor disabled");
      break;
    case '^':
      if(switchThemeEditor()) remoteSetColorTheme(stream);
      break;
    case '@':
      if(switchThemeEditor()) remoteGetColorTheme(stream);
      break;

    default:
      // Command not recognized
      return(event);
  }

  // Command recognized
  return(event | REMOTE_CHANGED);
}

static int serialLoop(Stream* stream, RemoteState* state, uint8_t usbMode)
{
  if(usbMode == USB_OFF) return 0;

  remoteTickTime(stream, state);

  if (stream->available())
    return remoteDoCommand(stream, state, stream->read());
  return 0;
}

int serialLoop(uint8_t usbMode)
{
  return serialLoop(&Serial, &remoteSerialState, usbMode);
}

bool serialConsumeAbortPending(uint8_t usbMode)
{
  if(usbMode == USB_OFF || !Serial.available()) return false;
  Serial.read();
  return true;
}
