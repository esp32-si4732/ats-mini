#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <TFT_eSPI.h>
#include <SI4735.h>

#define APP_ID        67   // EEPROM ID
#define APP_VERSION   200  // EEPROM VERSION

// Modes
#define FM            0
#define LSB           1
#define USB           2
#define AM            3

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

//
// Data Types
//

typedef struct
{
  const char *bandName;   // Band description
  uint8_t bandType;       // Band type (FM, MW, or SW)
  uint8_t bandMode;       // Band mode (FM, AM, LSB, or SSB)
  uint16_t minimumFreq;   // Minimum frequency of the band
  uint16_t maximumFreq;   // maximum frequency of the band
  uint16_t currentFreq;   // Default frequency or current frequency
  int8_t currentStepIdx;  // Index of stepAM[]: defeult frequency step (see stepAM[])
  int8_t bandwidthIdx;    // Index of the table bandwidthFM, bandwidthAM or bandwidthSSB;
  int16_t bandCal;        // Calibration value
} Band;

//
// Global Variables
//

extern SI4735 rx;
extern TFT_eSprite spr;
extern TFT_eSPI tft;

extern bool tuning_flag;
extern uint8_t rssi;

extern uint16_t currentFrequency;
extern int16_t currentBFO;
extern uint8_t currentMode;
extern uint16_t currentCmd;
extern uint16_t currentBrt;
extern uint16_t currentSleep;
extern uint8_t AmTotalSteps;

extern int8_t FmAgcIdx;
extern int8_t AmAgcIdx;
extern int8_t SsbAgcIdx;
extern int8_t AmAvcIdx;
extern int8_t SsbAvcIdx;
extern int8_t AmSoftMuteIdx;
extern int8_t SsbSoftMuteIdx;

extern int8_t agcIdx;
extern int8_t agcNdx;
extern int8_t softMuteMaxAttIdx;
extern uint8_t disableAgc;

extern const int CALMax;

static inline bool isSSB() { return(currentMode>FM && currentMode<AM); }

void useBand(const Band *band);
void updateBFO(int newBFO);

// Utils.c
void loadSSB(uint8_t bandwidth);
void unloadSSB();
const char *getVersion();
int getStrength(int rssi);
bool displayOn(int x = 2);
bool muteOn(int x = 2);
const char *clockGet();
bool clockSet(uint8_t hours, uint8_t minutes, uint8_t seconds = 0);
bool clockTickTime();

// Draw.c
void drawLoadingSSB();
void drawScreen();

// Battery.c
float batteryMonitor();
void drawBattery(int x, int y);

// Station.c
const char *getStationName();
const char *getStationInfo();
const char *getProgramInfo();
const char *getRdsTime();
uint16_t getRdsPiCode();
void clearStationInfo();
bool checkRds();
bool checkCbChannel();

#ifndef DISABLE_REMOTE
// Remote.c
void remoteTickTime();
bool remoteDoCommand(char key);
#endif

#endif // COMMON_H
