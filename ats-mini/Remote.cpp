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

//
// Capture current screen image to the remote
//
static void remoteCaptureScreen(Stream* stream)
{
  uint16_t width  = spr.width();
  uint16_t height = spr.height();

  // Read the sprite framebuffer directly instead of calling readPixel()+printf()
  // 54,400 times. spr.getPointer() returns the raw 16bpp framebuffer base; the
  // sprite is created unrotated as 320x170 with no sub-viewport, so pixel (x,y)
  // is at fb[x + y*width]. The stored word equals htons(spr.readPixel(x,y)), so
  // emitting it as four lowercase hex digits is byte-for-byte identical to the
  // previous per-pixel printf("%04x", htons(...)) output.
  const uint16_t *fb = (const uint16_t *)spr.getPointer();
  if(!fb) return;

  // 14 bytes of BMP header
  stream->println("");
  stream->print("424d"); // BM
  // Image size
  stream->printf("%08x", (unsigned int)htonl(14 + 40 + 12 + width * height * 2));
  stream->print("00000000");
  // Offset to image data
  stream->printf("%08x", (unsigned int)htonl(14 + 40 + 12));
  // Image header
  stream->print("28000000"); // Header size
  stream->printf("%08x", (unsigned int)htonl(width));
  stream->printf("%08x", (unsigned int)htonl(height));
  stream->print("01001000"); // 1 plane, 16 bpp
  stream->print("03000000"); // Compression
  stream->print("00000000"); // Compressed image size
  stream->print("00000000"); // X res
  stream->print("00000000"); // Y res
  stream->print("00000000"); // Color map
  stream->print("00000000"); // Colors
  stream->print("00f80000"); // Red mask
  stream->print("e0070000"); // Green mask
  stream->println("1f000000"); // Blue mask

  // Image data: build each row in one buffer and emit it with a single write().
  // The 320x170 sprite yields 1280 hex chars + CRLF per row; rowBuf lives in
  // .bss (not on the cooperative loop's stack) and is safe because this path is
  // single-entrant (reached only from remoteDoCommand()'s 'C' case).
  static const char hex[] = "0123456789abcdef";
  static char rowBuf[320 * 4 + 2];
  for(int y=height-1 ; y>=0 ; y--)
  {
    const uint16_t *row = fb + (uint32_t)y * width;
    char *p = rowBuf;
    for(int x=0 ; x<width ; x++)
    {
      uint16_t v = row[x]; // == htons(spr.readPixel(x, y))
      *p++ = hex[(v >> 12) & 0xF];
      *p++ = hex[(v >>  8) & 0xF];
      *p++ = hex[(v >>  4) & 0xF];
      *p++ = hex[ v        & 0xF];
    }
    *p++ = '\r'; *p++ = '\n'; // matches println("")
    stream->write((const uint8_t *)rowBuf, p - rowBuf);
  }
  stream->flush();
}

//
// Capture the screen as a raw little-endian RGB565 BMP (command 'c').
//
// Additive, opt-in alternative to 'C': it emits ~2x fewer bytes (a binary BMP
// instead of ASCII hex) and does no per-pixel formatting. The decoded image is
// byte-for-byte identical to xxd-decoding the 'C' output. A leading
// "BMP:<size>\r\n" ASCII frame lets line-oriented consumers find the binary
// start and pre-allocate.
//
static void remoteCaptureScreenBinary(Stream* stream)
{
  uint16_t width  = spr.width();
  uint16_t height = spr.height();
  const uint16_t *fb = (const uint16_t *)spr.getPointer();
  if(!fb) return;

  uint32_t fileSize  = 14 + 40 + 12 + (uint32_t)width * height * 2;
  uint32_t pixOffset = 14 + 40 + 12;
  uint32_t hsz = 40, compr = 3;
  uint32_t w = width, ht = height;
  uint32_t rm = 0xF800, gm = 0x07E0, bm = 0x001F;
  uint16_t planes = 1, bpp = 16;

  // BMP fields are little-endian; on the little-endian ESP32 write them in
  // native order (do NOT reuse the htonl() trick the hex 'C' path uses).
  uint8_t h[66];
  h[0] = 'B'; h[1] = 'M';
  memcpy(h + 2, &fileSize, 4);
  memset(h + 6, 0, 4);
  memcpy(h + 10, &pixOffset, 4);
  memcpy(h + 14, &hsz, 4);
  memcpy(h + 18, &w, 4);
  memcpy(h + 22, &ht, 4);
  memcpy(h + 26, &planes, 2);
  memcpy(h + 28, &bpp, 2);
  memcpy(h + 30, &compr, 4);
  memset(h + 34, 0, 20);
  memcpy(h + 54, &rm, 4);
  memcpy(h + 58, &gm, 4);
  memcpy(h + 62, &bm, 4);

  stream->printf("BMP:%u\r\n", (unsigned int)fileSize);
  stream->write(h, sizeof(h));

  // Pixels, bottom-up. The framebuffer stores each RGB565 word byte-swapped
  // (TFT_eSprite::drawPixel does color=(color>>8)|(color<<8) at 16bpp), so emit
  // the high byte first to produce the little-endian RGB565 a BI_BITFIELDS BMP
  // expects. These bytes equal xxd-decoding the 'C' output.
  static uint8_t bmpRow[320 * 2];
  for(int y=height-1 ; y>=0 ; y--)
  {
    const uint16_t *row = fb + (uint32_t)y * width;
    uint8_t *p = bmpRow;
    for(int x=0 ; x<width ; x++)
    {
      uint16_t v = row[x];
      *p++ = (uint8_t)(v >> 8);
      *p++ = (uint8_t)(v & 0xFF);
    }
    stream->write(bmpRow, p - bmpRow);
  }
  stream->flush();
}

char remoteReadChar(Stream* stream)
{
  char key;

  // Bound the wait so a BLE disconnect mid-command (e.g. during '#' or '^')
  // cannot spin the cooperative loop forever and trip the task watchdog. The
  // deadline never fires on USB or on a well-behaved BLE host that sends the
  // whole command at once; delay(1) yields to the scheduler while waiting.
  uint32_t deadline = millis() + 2000;
  while (!stream->available())
  {
    if ((int32_t)(millis() - deadline) >= 0) return 0; // abort signal
    delay(1);
  }
  key = stream->read();
  stream->print(key);
  return key;
}

// Wait until a byte is available, then return it (via peek(), without consuming)
// as an int; return -1 if none arrives within the deadline. Multi-byte commands
// can dribble in over several loop ticks when typed in a terminal, so the parsers
// must block here rather than give up on a momentarily empty buffer. The deadline
// + delay(1) replace the previous unbounded busy-spin (`while(peek()==0xFF)`),
// which on this toolchain (char is unsigned on Xtensa, so peek()==-1 became 0xFF)
// would loop forever on a BLE disconnect and trip the task watchdog.
static int remotePeekBlocking(Stream* stream)
{
  uint32_t deadline = millis() + 2000;
  int peeked;
  while ((peeked = stream->peek()) < 0)
  {
    if ((int32_t)(millis() - deadline) >= 0) return -1;
    delay(1);
  }
  return peeked;
}

long int remoteReadInteger(Stream* stream)
{
  long int result = 0;
  while (true) {
    int peeked = remotePeekBlocking(stream);
    if (peeked < 0) {
      return result;
    }
    char ch = (char)peeked;
    if ((ch >= '0') && (ch <= '9')) {
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
    int peeked = remotePeekBlocking(stream);
    if (peeked < 0 || (char)peeked == ',' || (char)peeked < ' ') {
      bufStr[length] = '\0';
      return;
    }
    char ch = remoteReadChar(stream);
    bufStr[length] = ch;
    if (++length >= bufLen - 1) {
      bufStr[length] = '\0';
      return;
    }
  }
}

static bool expectNewline(Stream* stream)
{
  if (remotePeekBlocking(stream) == '\r') {
    stream->read();
    return true;
  }
  return false;
}

static bool remoteShowError(Stream* stream, const char *message)
{
  // Drain the remaining input without echoing it (remoteReadChar() echoes each
  // byte, which over BLE means an extra write() per leftover byte before the
  // error message, and pollutes the stream for line-oriented consumers).
  while (stream->available()) stream->read();
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

//
// Emit one populated memory slot per call. The "$" command starts the dump by
// setting state->memoryDumpSlot = 0; this is then driven once per loop tick so
// a BLE transfer (~2 KB/s) hands out one ~35-byte slot at a time instead of
// blocking the cooperative loop for ~1.7 s. The wire format is unchanged.
//
void remoteMemoryDumpTick(Stream* stream, RemoteState* state)
{
  if (state->memoryDumpSlot < 0) return;

  while (state->memoryDumpSlot < getTotalMemories()) {
    uint8_t i = state->memoryDumpSlot++;
    if (memories[i].freq) {
      stream->printf("#%02d,%s,%ld,%s\r\n", i + 1, bands[memories[i].band].bandName, memories[i].freq, bandModeDesc[memories[i].mode]);
      return;
    }
  }
  state->memoryDumpSlot = -1;
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
  uint32_t now = millis();
  if(state->remoteLogOn && (now - state->remoteTimer >= 500))
  {
    // Mark time and increment diagnostic sequence number
    state->remoteTimer = now;
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
      state->remoteLogOn = false;
      remoteCaptureScreen(stream);
      break;
    case 'c':
      state->remoteLogOn = false;
      remoteCaptureScreenBinary(stream);
      break;
    case 't':
      state->remoteLogOn = !state->remoteLogOn;
      break;

    case '$':
      // Start a chunked dump; slots are emitted by remoteMemoryDumpTick().
      state->memoryDumpSlot = 0;
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
  remoteMemoryDumpTick(stream, state);

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
