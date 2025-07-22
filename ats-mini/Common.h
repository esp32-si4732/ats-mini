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
#define APP_VERSION    228  // FIRMWARE VERSION
#define EEPROM_VERSION 71   // EEPROM VERSION (forces reset)

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

// Compute number of items in an array
#define ITEM_COUNT(array) (sizeof(array) / sizeof((array)[0]))
#define LAST_ITEM(array)  (ITEM_COUNT(array) - 1)

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
#define BLE_BLUEFRUIT  1 // Bluefruit Connect app mode

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
  uint16_t freq;          // Frequency
  uint8_t  band;          // Band
  uint8_t  mode  : 4;     // Modulation
  uint8_t  hz100 : 4;     // Hz * 100
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
  const char *city;       // City name
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

extern bool tuning_flag;
extern bool pushAndRotate;
extern uint8_t rssi;
extern uint8_t snr;

extern uint8_t volume;
extern uint8_t currentSquelch;
extern bool squelchCutoff;
extern uint16_t currentFrequency;
extern int16_t currentBFO;
extern uint8_t currentMode;
extern uint16_t currentCmd;
extern uint16_t currentBrt;
extern uint16_t currentSleep;
extern uint8_t sleepModeIdx;
extern bool zoomMenu;
extern int8_t scrollDirection;
extern uint8_t utcOffsetIdx;
extern uint8_t uiLayoutIdx;

extern int8_t FmAgcIdx;
extern int8_t AmAgcIdx;
extern int8_t SsbAgcIdx;
extern int8_t AmAvcIdx;
extern int8_t SsbAvcIdx;
extern int8_t AmSoftMuteIdx;
extern int8_t SsbSoftMuteIdx;
extern uint8_t rdsModeIdx;
extern uint8_t bleModeIdx;
extern uint8_t wifiModeIdx;
extern uint8_t FmRegionIdx;

extern int8_t agcIdx;
extern int8_t agcNdx;
extern int8_t softMuteMaxAttIdx;
extern uint8_t disableAgc;

extern const int CALMax;

static inline bool isSSB() { return(currentMode>FM && currentMode<AM); }

void useBand(const Band *band);
bool updateBFO(int newBFO, bool wrap = true);
bool doSeek(int8_t dir);
bool clickFreq(bool shortPress);
uint8_t doAbout(int dir);

// Battery.c
float batteryMonitor();
bool drawBattery(int x, int y);

// Scan.c
void scanRun(uint16_t centerFreq, uint16_t step);
float scanGetRSSI(uint16_t freq);
float scanGetSNR(uint16_t freq);

// Station.c
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
void netClearPreferences();
void netInit(uint8_t netMode, bool showStatus = true);
void netStop();
bool ntpIsAvailable();
bool ntpSyncTime();

void netRequestConnect();
void netTickTime();

// Ble.cpp
int bleDoCommand(uint8_t bleModeIdx);
void bleInit(uint8_t bleMode);
void bleStop();
int8_t getBleStatus();

#ifndef DISABLE_REMOTE
// Remote.c
#define REMOTE_CHANGED   1
#define REMOTE_CLICK     2
#define REMOTE_EEPROM    4
#define REMOTE_DIRECTION 8
void remoteTickTime();
int remoteDoCommand(char key);
char readSerialChar();
#endif

#endif // COMMON_H
