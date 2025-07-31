#include "Common.h"
#include "Themes.h"
#include "Utils.h"
#include "Menu.h"
#include "Draw.h"

#ifndef DISABLE_REMOTE

static uint32_t remoteTimer = millis();
static uint8_t remoteSeqnum = 0;
static bool remoteLogOn = false;

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
static void remoteCaptureScreen()
{
  uint16_t width  = spr.width();
  uint16_t height = spr.height();

  // 14 bytes of BMP header
  Serial.println("");
  Serial.print("424d"); // BM
  // Image size
  Serial.printf("%08x", (unsigned int)htonl(14 + 40 + 12 + width * height * 2));
  Serial.print("00000000");
  // Offset to image data
  Serial.printf("%08x", (unsigned int)htonl(14 + 40 + 12));
  // Image header
  Serial.print("28000000"); // Header size
  Serial.printf("%08x", (unsigned int)htonl(width));
  Serial.printf("%08x", (unsigned int)htonl(height));
  Serial.print("01001000"); // 1 plane, 16 bpp
  Serial.print("03000000"); // Compression
  Serial.print("00000000"); // Compressed image size
  Serial.print("00000000"); // X res
  Serial.print("00000000"); // Y res
  Serial.print("00000000"); // Color map
  Serial.print("00000000"); // Colors
  Serial.print("00f80000"); // Red mask
  Serial.print("e0070000"); // Green mask
  Serial.println("1f000000"); // Blue mask

  // Image data
  for(int y=height-1 ; y>=0 ; y--)
  {
    for(int x=0 ; x<width ; x++)
    {
      Serial.printf("%04x", htons(spr.readPixel(x, y)));
    }
    Serial.println("");
  }
}

char readSerialChar()
{
  char key;

  while (!Serial.available());
  key = Serial.read();
  Serial.print(key);
  return key;
}

long int readSerialInteger()
{
  long int result = 0;
  while (true) {
    char ch = Serial.peek();
    if (ch == 0xFF) {
      continue;
    } else if ((ch >= '0') && (ch <= '9')) {
      ch = readSerialChar();
      // Can overflow, but it's ok
      result = result * 10 + (ch - '0');
    } else {
      return result;
    }
  }
}

void readSerialString(char *bufStr, uint8_t bufLen)
{
  uint8_t length = 0;
  while (true) {
    char ch = Serial.peek();
    if (ch == 0xFF) {
      continue;
    } else if (ch == ',' || ch < ' ') {
      bufStr[length] = '\0';
      return;
    } else {
      ch = readSerialChar();
      bufStr[length] = ch;
      if (++length >= bufLen - 1) {
        bufStr[length] = '\0';
        return;
      }
    }
  }
}

static bool expectNewline()
{
  char ch;
  while ((ch = Serial.peek()) == 0xFF);
  if (ch == '\r') {
    Serial.read();
    return true;
  }
  return false;
}

static bool showError(const char *message)
{
  // Consume the remaining input
  while (Serial.available()) readSerialChar();
  Serial.printf("\r\nError: %s\r\n", message);
  return false;
}

static void remoteGetMemories()
{
  for (uint8_t i = 0; i < getTotalMemories(); i++) {
    if (memories[i].freq) {
      uint32_t freq = (memories[i].mode == LSB || memories[i].mode == USB) ?
        memories[i].freq * 1000 + memories[i].hz100 * 100 :
        (memories[i].mode == AM) ? memories[i].freq * 1000 : memories[i].freq * 10000;
      Serial.printf("#%02d,%s,%ld,%s\r\n", i + 1, bands[memories[i].band].bandName, freq, bandModeDesc[memories[i].mode]);
    }
  }
}


static bool remoteSetMemory()
{
  Serial.print('#');
  Memory mem;
  uint32_t freq = 0;

  long int slot = readSerialInteger();
  if (readSerialChar() != ',')
    return showError("Expected ','");
  if (slot < 1 || slot > getTotalMemories())
    return showError("Invalid memory slot number");

  char band[8];
  readSerialString(band, 8);
  if (readSerialChar() != ',')
    return showError("Expected ','");
  mem.band = 0xFF;
  for (int i = 0; i < getTotalBands(); i++) {
    if (strcmp(bands[i].bandName, band) == 0) {
      mem.band = i;
      break;
    }
  }
  if (mem.band == 0xFF)
    return showError("No such band");

  freq = readSerialInteger();
  if (readSerialChar() != ',')
    return showError("Expected ','");

  char mode[4];
  readSerialString(mode, 4);
  if (!expectNewline())
    return showError("Expected newline");
  Serial.println();
  mem.mode = 15;
  for (int i = 0; i < getTotalModes(); i++) {
    if (strcmp(bandModeDesc[i], mode) == 0) {
      mem.mode = i;
      break;
    }
  }
  if (mem.mode == 15)
    return showError("No such mode");

  if (mem.mode == LSB || mem.mode == USB) {
    mem.freq = freq / 1000;
    mem.hz100 = (freq % 1000) / 100;
  } else if (mem.mode == AM) {
    mem.freq = freq / 1000;
    mem.hz100 = 0;
  } else {
    mem.freq = freq / 10000;
    mem.hz100 = 0;
  }

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
        return showError("No such band");
      if (!isMemoryInBand(&bands[mem.band], &mem))
        return showError("Invalid frequency or mode");
    }
  }

  memories[slot-1] = mem;
  return true;
}

//
// Set current color theme from the remote
//
static void remoteSetColorTheme()
{
  Serial.print("Enter a string of hex colors (x0001x0002...): ");

  uint8_t *p = (uint8_t *)&(TH.bg);

  for(int i=0 ; ; i+=sizeof(uint16_t))
  {
    if(i >= sizeof(ColorTheme)-offsetof(ColorTheme, bg))
    {
      Serial.println(" Ok");
      break;
    }

    if(readSerialChar() != 'x')
    {
      Serial.println(" Err");
      break;
    }

    p[i + 1]  = char2nibble(readSerialChar()) * 16;
    p[i + 1] |= char2nibble(readSerialChar());
    p[i]      = char2nibble(readSerialChar()) * 16;
    p[i]     |= char2nibble(readSerialChar());
  }

  // Redraw screen
  drawScreen();
}

//
// Print current color theme to the remote
//
static void remoteGetColorTheme()
{
  Serial.printf("Color theme %s: ", TH.name);
  const uint8_t *p = (uint8_t *)&(TH.bg);

  for(int i=0 ; i<sizeof(ColorTheme)-offsetof(ColorTheme, bg) ; i+=sizeof(uint16_t))
  {
    Serial.printf("x%02X%02X", p[i+1], p[i]);
  }

  Serial.println();
}

//
// Print current status to the remote
//
void remotePrintStatus()
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
  Serial.printf("%u,%u,%d,%d,%s,%s,%s,%s,%hu,%hu,%hu,%hu,%hu,%.2f,%hu\r\n",
                APP_VERSION,
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
                remoteSeqnum
                );
}

//
// Tick remote time, periodically printing status
//
void remoteTickTime()
{
  if(remoteLogOn && (millis() - remoteTimer >= 500))
  {
    // Mark time and increment diagnostic sequence number
    remoteTimer = millis();
    remoteSeqnum++;
    // Show status
    remotePrintStatus();
  }
}

//
// Recognize and execute given remote command
//
int remoteDoCommand(char key)
{
  int event = 0;

  switch(key)
  {
    case 'R': // Rotate Encoder Clockwise
      event |= 1 << REMOTE_DIRECTION;
      event |= REMOTE_EEPROM;
      break;
    case 'r': // Rotate Encoder Counterclockwise
      event |= -1 << REMOTE_DIRECTION;
      event |= REMOTE_EEPROM;
      break;
    case 'e': // Encoder Push Button
      event |= REMOTE_CLICK;
      break;
    case 'B': // Band Up
      doBand(1);
      event |= REMOTE_EEPROM;
      break;
    case 'b': // Band Down
      doBand(-1);
      event |= REMOTE_EEPROM;
      break;
    case 'M': // Mode Up
      doMode(1);
      event |= REMOTE_EEPROM;
      break;
    case 'm': // Mode Down
      doMode(-1);
      event |= REMOTE_EEPROM;
      break;
    case 'S': // Step Up
      doStep(1);
      event |= REMOTE_EEPROM;
      break;
    case 's': // Step Down
      doStep(-1);
      event |= REMOTE_EEPROM;
      break;
    case 'W': // Bandwidth Up
      doBandwidth(1);
      event |= REMOTE_EEPROM;
      break;
    case 'w': // Bandwidth Down
      doBandwidth(-1);
      event |= REMOTE_EEPROM;
      break;
    case 'A': // AGC/ATTN Up
      doAgc(1);
      event |= REMOTE_EEPROM;
      break;
    case 'a': // AGC/ATTN Down
      doAgc(-1);
      event |= REMOTE_EEPROM;
      break;
    case 'V': // Volume Up
      doVolume(1);
      event |= REMOTE_EEPROM;
      break;
    case 'v': // Volume Down
      doVolume(-1);
      event |= REMOTE_EEPROM;
      break;
    case 'L': // Backlight Up
      doBrt(1);
      event |= REMOTE_EEPROM;
      break;
    case 'l': // Backlight Down
      doBrt(-1);
      event |= REMOTE_EEPROM;
      break;
    case 'O':
      sleepOn(true);
      break;
    case 'o':
      sleepOn(false);
      break;
    case 'I':
      doCal(1);
      event |= REMOTE_EEPROM;
      break;
    case 'i':
      doCal(-1);
      event |= REMOTE_EEPROM;
      break;
    case 'C':
      remoteLogOn = false;
      remoteCaptureScreen();
      break;
    case 't':
      remoteLogOn = !remoteLogOn;
      break;

    case '$':
      remoteGetMemories();
      break;
    case '#':
      if (remoteSetMemory())
        event |= REMOTE_EEPROM;
      break;

    case 'T':
      Serial.println(switchThemeEditor(!switchThemeEditor()) ? "Theme editor enabled" : "Theme editor disabled");
      break;
    case '!':
      if(switchThemeEditor()) remoteSetColorTheme();
      break;
    case '@':
      if(switchThemeEditor()) remoteGetColorTheme();
      break;

    default:
      // Command not recognized
      return(event);
  }

  // Command recognized
  return(event | REMOTE_CHANGED);
}

#endif // !DISABLE_REMOTE
