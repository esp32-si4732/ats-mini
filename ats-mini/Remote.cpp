#include "Common.h"
#include "Themes.h"
#include "Utils.h"
#include "Menu.h"
#include "Draw.h"
#include "Remote.h"


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

  // Image data
  for(int y=height-1 ; y>=0 ; y--)
  {
    for(int x=0 ; x<width ; x++)
    {
      stream->printf("%04x", htons(spr.readPixel(x, y)));
    }
    stream->println("");
  }
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

int serialDoCommand(Stream* stream, RemoteState* state, uint8_t usbMode)
{
  if(usbMode == USB_OFF) return 0;

  if (Serial.available())
    return remoteDoCommand(stream, state, Serial.read());
  return 0;
}

void serialTickTime(Stream* stream, RemoteState* state, uint8_t usbMode)
{
  if(usbMode == USB_OFF) return;

  remoteTickTime(stream, state);
}
