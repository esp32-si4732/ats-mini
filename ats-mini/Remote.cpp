#include "Common.h"
#include "Themes.h"
#include "Utils.h"
#include "Menu.h"
#include "Draw.h"
#include "Remote.h"

static RemoteState remoteSerialState;

// The row buffer below is sized for the receiver's 320px-wide sprite. Both
// captures bail out rather than overflow it if that ever grows.
#define REMOTE_MAX_SCREEN_WIDTH 320

// Screenshot row buffer shared by both captures, sized for the larger hex row
// (4 digits per pixel plus CRLF; a binary row takes 2 bytes per pixel).
// Allocated in PSRAM on first use and kept: internal DRAM is scarce, and
// building the rows in PSRAM costs about 1% of the capture time.
static uint8_t *remoteRowBuf = nullptr;

static uint8_t *remoteGetRowBuf()
{
  if(!remoteRowBuf)
    remoteRowBuf = static_cast<uint8_t *>(ps_malloc(REMOTE_MAX_SCREEN_WIDTH * 4 + 2));
  return remoteRowBuf;
}

//
// Bulk output over the USB CDC (Serial), used for the screenshots.
//
// HWCDC throws queued bytes away when the USB SOF watchdog briefly reports the
// cable unplugged, which now and then happens on a healthy link too: write()
// then evicts the oldest bytes in its 256 B TX ring to make room, and flush()
// empties the ring. The old per-pixel loop never filled the ring, so it never
// lost anything, but a fast capture keeps it full, and 1-2% of captures lost
// a 1-21 KB chunk. Handing write() no more than the ring has room for leaves
// it nothing to evict, and there is no need to flush(). Topping up the ring as
// it drains is also 7-10% faster than blocking in write() until a whole chunk
// fits.
//
class UsbBulkStream : public Stream
{
  bool stalled = false;

public:
  int available() override { return Serial.available(); }
  int read() override { return Serial.read(); }
  int peek() override { return Serial.peek(); }
  void flush() override {}
  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t *buf, size_t size) override
  {
    size_t done = 0;
    uint32_t lastProgress = millis();
    while(!stalled && done < size)
    {
      int room = Serial.availableForWrite();
      size_t n = 0;
      if(room > 0)
        n = Serial.write(buf + done, (size - done) < (size_t)room ? (size - done) : (size_t)room);
      if(n)
      {
        done += n;
        lastProgress = millis();
      }
      // Nothing drains with the cable out or the host not reading: drop the
      // rest of the capture rather than hold up the main loop
      else if(millis() - lastProgress > 500)
        stalled = true;
    }
    return done;
  }
};

//
// Run a screenshot capture, through UsbBulkStream when it goes to Serial
//
static void remoteScreenshot(Stream* stream, void (*capture)(Stream*))
{
  UsbBulkStream usb;
  capture(stream == &Serial ? &usb : stream);
}

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
  // 54,400 times. spr.getBuffer() returns the raw 16bpp framebuffer base. The
  // sprite is created unrotated as 320x170 at rgb565_2Byte depth, so the row
  // stride equals the width and pixel (x,y) is at fb[x + y*width]. LovyanGFX
  // stores that depth as swap565_t (byte-swapped), and readPixel() swaps it
  // back, so the stored word equals htons(spr.readPixel(x,y)): emitting it as
  // four lowercase hex digits is byte-for-byte identical to the previous
  // per-pixel printf("%04x", htons(...)) output.
  const uint16_t *fb = (const uint16_t *)spr.getBuffer();
  char *rowBuf = (char *)remoteGetRowBuf();
  if(!fb || !rowBuf || width > REMOTE_MAX_SCREEN_WIDTH) return;

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
  // The 320x170 sprite yields 1280 hex chars + CRLF per row. Keeping one rowBuf
  // is safe because captures are single-entrant (reached only from
  // remoteDoCommand()).
  static const char hex[] = "0123456789abcdef";
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
    // Matches the println("") the old loop ended each row with
    *p++ = '\r';
    *p++ = '\n';
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
  const uint16_t *fb = (const uint16_t *)spr.getBuffer();
  uint8_t *bmpRow = remoteGetRowBuf();
  if(!fb || !bmpRow || width > REMOTE_MAX_SCREEN_WIDTH) return;

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
  // (LovyanGFX keeps rgb565_2Byte sprites in swap565_t form), so emit the high
  // byte first to produce the little-endian RGB565 a BI_BITFIELDS BMP expects.
  // These bytes equal xxd-decoding the 'C' output.
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
      state->remoteLogOn = false;
      remoteScreenshot(stream, remoteCaptureScreen);
      break;
    case 'c':
      state->remoteLogOn = false;
      remoteScreenshot(stream, remoteCaptureScreenBinary);
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
