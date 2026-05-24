#ifndef MENU_H
#define MENU_H

#include "Common.h"
#include "MenuData.h"

// Number of memory slots
#define MEMORY_COUNT  99

// Band Types
#define FM_BAND_TYPE  0
#define MW_BAND_TYPE  1
#define SW_BAND_TYPE  2
#define LW_BAND_TYPE  3

// Commands
#define CMD_NONE       0x0000
#define CMD_FREQ       0x0100
#define CMD_BAND       0x1000 //-MENU MODE starts here
#define CMD_VOLUME     0x1100 // |
#define CMD_AGC        0x1200 // |
#define CMD_BANDWIDTH  0x1300 // |
#define CMD_STEP       0x1400 // |
#define CMD_MODE       0x1500 // |
#define CMD_MENU       0x1600 // |
#define CMD_SOFTMUTE   0x1700 // |
#define CMD_AVC        0x1800 // |
#define CMD_MEMORY     0x1900 // |
#define CMD_SEEK       0x1A00 // |
#define CMD_SCAN       0x1B00 // |
#define CMD_SQUELCH    0x1C00 //-+
#define CMD_SETTINGS   0x2000 //-SETTINGS MODE starts here
#define CMD_BRT        0x2100 // |
#define CMD_CAL        0x2200 // |
#define CMD_RDS        0x2300 // |
#define CMD_UTCOFFSET  0x2400 // |
#define CMD_FM_REGION  0x2500 // |
#define CMD_THEME      0x2600 // |
#define CMD_UI         0x2700 // |
#define CMD_ZOOM       0x2800 // |
#define CMD_SCROLL     0x2900 // |
#define CMD_SLEEP      0x2A00 // |
#define CMD_SLEEPMODE  0x2B00 // |
#define CMD_LOADEIBI   0x2C00 // |
#define CMD_USBMODE    0x2D00 // |
#define CMD_BLEMODE    0x2E00 // |
#define CMD_WIFIMODE   0x2F00 // |
#define CMD_ABOUT      0x3000 //-+

// UI Layouts
#define UI_DEFAULT  0
#define UI_SMETER   1

// Seek modes
#define SEEK_DEFAULT  0
#define SEEK_SCHEDULE 1

// Main menu item indices
#define MENU_MODE         0
#define MENU_BAND         1
#define MENU_VOLUME       2
#define MENU_STEP         3
#define MENU_SEEK         4
#define MENU_SCAN         5
#define MENU_MEMORY       6
#define MENU_SQUELCH      7
#define MENU_BW           8
#define MENU_AGC_ATT      9
#define MENU_AVC         10
#define MENU_SOFTMUTE    11
#define MENU_SETTINGS    12

// Settings menu item indices
#define MENU_BRIGHTNESS   0
#define MENU_CALIBRATION  1
#define MENU_RDS          2
#define MENU_UTCOFFSET    3
#define MENU_FM_REGION    4
#define MENU_THEME        5
#define MENU_UI           6
#define MENU_ZOOM         7
#define MENU_SCROLL       8
#define MENU_SLEEP        9
#define MENU_SLEEPMODE    10
#define MENU_LOADEIBI     11
#define MENU_USBMODE      12
#define MENU_BLEMODE      13
#define MENU_WIFIMODE     14
#define MENU_ABOUT        15

//
// Data Types
//

//
// Mutable State
//
extern int bandIdx;
extern int8_t menuIdx;
extern int8_t settingsIdx;
extern uint8_t memoryIdx;

//
// Menu Draw Handlers
//
static inline bool isMenuMode(uint16_t cmd)
{
  return((cmd>=CMD_BAND) && (cmd<CMD_SETTINGS));
}

// These are settings
static inline bool isSettingsMode(uint16_t cmd)
{
  return((cmd>=CMD_SETTINGS) && (cmd<CMD_ABOUT));
}

uint8_t seekMode(bool toggle = false);
bool doSideBar(uint16_t cmd, int16_t enc, int16_t enca);
void doSelectDigit(int16_t enc);
bool clickHandler(uint16_t cmd, bool shortPress);
void selectBand(uint8_t idx, bool drawLoadingSSB = true);
int getTotalBands();
int getTotalModes();
int getTotalMemories();
int getTotalMenuItems();
int getTotalSettingsItems();
int getTotalRDSModes();
int getTotalSleepModes();
int getTotalUILayouts();
int getTotalWiFiModes();
int getLastStep(int mode);
int getLastBandwidth(int mode);
Band *getCurrentBand();
uint8_t getFreqInputPos();
int getFreqInputStep();
const Step *getCurrentStep();
const Bandwidth *getCurrentBandwidth();
uint8_t getRDSMode();

int getCurrentUTCOffset();
int getTotalUTCOffsets();
int getTotalFmRegions();
int getTotalUSBModes();
int getTotalBleModes();

void doSoftMute(int16_t enc);
void doAgc(int16_t enc);
void doAvc(int16_t enc);
void doFmRegion(int16_t enc);
void doBandwidth(int16_t enc);
void doVolume(int16_t enc);
void doBrt(int16_t enc);
void doCal(int16_t enc);
void doStep(int16_t enc);
void doMode(int16_t enc);
void doBand(int16_t enc);

#endif // MENU_H
