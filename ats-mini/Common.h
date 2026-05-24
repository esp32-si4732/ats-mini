#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <TFT_eSPI.h>
#include <SI4735-fixed.h>

#define RECEIVER_DESC  "ESP32-SI4732 Receiver"
#define RECEIVER_NAME  "ATS-Mini"
#define FIRMWARE_URL   "https://github.com/esp32-si4732/ats-mini"
#define MANUAL_URL     "https://esp32-si4732.github.io/ats-mini/manual.html"
#define AUTHORS_LINE1  "Authors: PU2CLR (Ricardo Caratti),"
#define AUTHORS_LINE2  "Volos Projects, Ralph Xavier, Sunnygold,"
#define AUTHORS_LINE3  "Goshante, G8PTN (Dave), R9UCL (Max Arnold),"
#define AUTHORS_LINE4  "Marat Fayzullin"

#define VER_APP        235  // Firmware version
#define VER_SETTINGS   71   // Settings version
#define VER_MEMORIES   71   // Memories version
#define VER_BANDS      72   // Bands version
#define VER_STORAGE     0   // LittleFS storage version

// Modes
#define FM            0
#define LSB           1
#define USB           2
#define AM            3

// RDS Modes
#define RDS_PS        0b00000001  // Station name
#define RDS_CT        0b00000010  // Time
#define RDS_PI        0b00000100  // PI code
#define RDS_RT        0b00001000  // Radio text
#define RDS_PT        0b00010000  // Program type
#define RDS_RBDS      0b00100000  // Use US PTYs

// Sleep modes
#define SLEEP_LOCKED   0 // Lock the encoder
#define SLEEP_UNLOCKED 1 // Do not lock the encoder
#define SLEEP_LIGHT    2 // ESP32 light sleep

#if defined(LILYGO_SI473X)

// SI4732/5 PINs for LilyGo T-Embed SI473x Shield
#define PIN_POWER_ON  46            // GPIO46   Board/shield power enable (1 = Enable)
#define RESET_PIN     16            // GPIO16   SI4732/5 Reset
#define ESP32_I2C_SCL  8            // GPIO8    SI4732/5 Clock
#define ESP32_I2C_SDA 18            // GPIO18   SI4732/5 Data
#define AUDIO_MUTE    17            // GPIO17   Hardware L/R mute, controlled via SI4735 code (1 = Mute)
#define PIN_AMP_EN    -1            // T-Embed shield has no separate amp enable pin

// Display PINs
#define PIN_LCD_BL    15            // GPIO15   LCD backlight (PWM brightness control)
// All other pins are defined by the TFT_eSPI library

// Rotary Enconder PINs
#define ENCODER_PIN_A  2            // GPIO02
#define ENCODER_PIN_B  1            // GPIO01
#define ENCODER_PUSH_BUTTON 0       // GPIO0

#else

// SI4732/5 PINs
#define PIN_POWER_ON  15            // GPIO15   External LDO regulator enable (1 = Enable)
#define RESET_PIN     16            // GPIO16   SI4732/5 Reset
#define ESP32_I2C_SCL 17            // GPIO17   SI4732/5 Clock
#define ESP32_I2C_SDA 18            // GPIO18   SI4732/5 Data
#define AUDIO_MUTE     3            // GPIO3    Hardware L/R mute, controlled via SI4735 code (1 = Mute)
#define PIN_AMP_EN    10            // GPIO10   Hardware Audio Amplifer enable (1 = Enable)

// Display PINs
#define PIN_LCD_BL    38            // GPIO38   LCD backlight (PWM brightness control)
// All other pins are defined by the TFT_eSPI library

// Rotary Enconder PINs
#define ENCODER_PIN_A  2            // GPIO02
#define ENCODER_PIN_B  1            // GPIO01
#define ENCODER_PUSH_BUTTON 21      // GPIO21

#endif

// Compute number of items in an array
#define ITEM_COUNT(array) (sizeof(array) / sizeof((array)[0]))
#define LAST_ITEM(array)  (ITEM_COUNT(array) - 1)

//
// Consolidated Radio State
//
typedef struct {
  // Tuning state
  uint16_t frequency;       // replaces currentFrequency
  uint8_t  mode;            // replaces currentMode
  int16_t  bfo;             // replaces currentBFO
  uint16_t cmd;             // replaces currentCmd
  bool     pnr;             // replaces pushAndRotate

  // Audio
  uint8_t vol;              // replaces volume
  uint8_t squelch[4];       // replaces currentSquelch[]

  // AGC/AVC/SoftMute per mode
  int8_t fmAgcIdx;          // replaces FmAgcIdx
  int8_t amAgcIdx;          // replaces AmAgcIdx
  int8_t ssbAgcIdx;         // replaces SsbAgcIdx
  int8_t amAvcIdx;          // replaces AmAvcIdx
  int8_t ssbAvcIdx;         // replaces SsbAvcIdx
  int8_t amSoftMuteIdx;     // replaces AmSoftMuteIdx
  int8_t ssbSoftMuteIdx;    // replaces SsbSoftMuteIdx
  int8_t agcIndex;          // replaces agcIdx
  int8_t agcNdxVal;         // replaces agcNdx
  int8_t softMuteMaxAtt;    // replaces softMuteMaxAttIdx
  uint8_t agcDisable;       // replaces disableAgc

  // Display
  uint16_t brightness;      // replaces currentBrt
  uint16_t sleep;           // replaces currentSleep
  uint8_t  sleepMode;       // replaces sleepModeIdx

  // Misc settings
  uint8_t  rdsMode;         // replaces rdsModeIdx
  uint8_t  usbMode;         // replaces usbModeIdx
  uint8_t  bleMode;         // replaces bleModeIdx
  uint8_t  wifiMode;        // replaces wifiModeIdx
  uint8_t  fmRegionIdx;     // replaces FmRegionIdx
  uint8_t  zoomLevel;       // replaces zoomMenu (was bool)
  int8_t   scrollDir;       // replaces scrollDirection (was uint8_t)
  int8_t   utcOffset;       // replaces utcOffsetIdx (was uint8_t)
  uint8_t  uiLayout;        // replaces uiLayoutIdx
} RadioState;

extern RadioState radioState;

// Periodic task timing (ms)
#define MIN_ELAPSED_RSSI_TIME  200  // RSSI check interval
#define RDS_CHECK_TIME         250  // RDS check interval (increased from 90)
#define NTP_CHECK_TIME       60000  // NTP time refresh period (ms)
#define SCHEDULE_CHECK_TIME   2000  // How often to identify the same frequency (ms)
#define BACKGROUND_REFRESH_TIME 5000  // Background screen refresh interval (ms)

// BFO and Calibration limits (MAX_BFO + MAX_CAL <= 16000)
#define MAX_BFO       14000  // Maximum range for currentBFO = +/- MAX_BFO
#define MAX_CAL       2000   // Maximum range for currentCAL = +/- MAX_CAL

// Network connection modes
#define NET_OFF        0 // Do not connect to the network
#define NET_AP_ONLY    1 // Create access point, do not connect to network
#define NET_AP_CONNECT 2 // Create access point, connect to a network normally, if possible
#define NET_CONNECT    3 // Connect to a network normally, if possible
#define NET_SYNC       4 // Connect to sync time, then disconnect

// Bluetooth modes
#define BLE_OFF        0 // Bluetooth is disabled
#define BLE_ADHOC      1 // Ad hoc BLE serial protocol
#define BLE_HID        2 // BLE HID central

// USB modes
#define USB_OFF        0 // USB is disabled
#define USB_ADHOC      1 // Ad hoc serial protocol

//
// Data Types
//

typedef struct
{
  const char *bandName;   // Band description
  uint8_t bandType;       // Band type (FM, MW, or SW)
  uint8_t bandMode;       // Band mode (FM, AM, LSB, or USB)
  uint16_t minimumFreq;   // Minimum frequency of the band
  uint16_t maximumFreq;   // Maximum frequency of the band
  uint16_t currentFreq;   // Default frequency or current frequency
  int8_t currentStepIdx;  // Default frequency step
  int8_t bandwidthIdx;    // Index of the table bandwidthFM, bandwidthAM or bandwidthSSB;
  int16_t usbCal;         // USB calibration value
  int16_t lsbCal;         // LSB calibration value
} Band;

typedef struct __attribute__((packed))
{
  uint32_t freq;          // Frequency (Hz)
  uint8_t  band;          // Band
  uint8_t  mode;          // Modulation
  char     name[10];      // Name
} Memory;

typedef struct
{
  uint16_t freq;          // Frequency
  const char *name;       // Frequency name
} NamedFreq;

typedef struct
{
  int8_t offset;          // UTC offset in 15 minute intervals
  const char *desc;       // Short description
} UTCOffset;

typedef struct
{
  // From https://www.skyworksinc.com/-/media/Skyworks/SL/documents/public/application-notes/AN332.pdf
  // Property 0x1100. FM_DEEMPHASIS
  uint8_t value;
  const char* desc;
} FMRegion;

//
// Global Variables
//

extern SI4735_fixed rx;
extern TFT_eSprite spr;
extern TFT_eSPI tft;

extern uint8_t rssi;
extern uint8_t snr;

extern volatile bool seekStop;

extern const int CALMax;

static inline bool isSSB() { return(radioState.mode>FM && radioState.mode<AM); }

void useBand(const Band *band);
bool updateFrequency(int newFreq, bool wrap = true);
bool updateBFO(int newBFO, bool wrap = true);
bool doSeek(int16_t enc);
bool clickFreq(bool shortPress);
uint8_t doAbout(int16_t enc);
bool consumeAbortPending();

// Battery.c
float batteryMonitor();
bool drawBattery(int x, int y);

// Scan.c
void scanRun(uint16_t centerFreq, uint16_t step);
float scanGetRSSI(uint16_t freq);
float scanGetSNR(uint16_t freq);

// Station.c
bool processRssiSnr();
const char *getStationName();
const char *getRadioText();
const char *getProgramInfo();
const char *getRdsTime();
uint16_t getRdsPiCode();
void clearStationInfo();
bool checkRds();
bool identifyFrequency(uint16_t freq, bool periodic = false);

// Network.cpp
int8_t getWiFiStatus();
char *getWiFiIPAddress();
void netInit(uint8_t netMode, bool showStatus = true);
void netStop();
bool ntpIsAvailable();
bool ntpSyncTime();

void netRequestConnect();
void netTickTime();

// Remote.c
#define REMOTE_CHANGED   1
#define REMOTE_CLICK     2
#define REMOTE_PREFS     4
#define REMOTE_SHORT_PRESS 8
#define REMOTE_PRESSED   16
#define REMOTE_DIRECTION 8

#endif // COMMON_H
