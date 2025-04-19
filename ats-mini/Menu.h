#ifndef MENU_H
#define MENU_H

#include "Common.h"

// Band Types
#define FM_BAND_TYPE  0
#define MW_BAND_TYPE  1
#define SW_BAND_TYPE  2
#define LW_BAND_TYPE  3

// Bands
#define BAND_VHF      0
#define BAND_MW1      1
#define BAND_MW2      2
#define BAND_MW3      3
#define BAND_80M      4
#define BAND_SW1      5
#define BAND_SW2      6
#define BAND_40M      7
#define BAND_SW3      8
#define BAND_SW4      9
#define BAND_SW5      10
#define BAND_SW6      11
#define BAND_20M      12
#define BAND_SW7      13
#define BAND_SW8      14
#define BAND_15M      15
#define BAND_SW9      16
#define BAND_CB       17
#define BAND_10M      18
#define BAND_ALL      19

// Commands
#define CMD_NONE      0x0000
#define CMD_BAND      0x0100 //-MENU MODE starts here
#define CMD_VOLUME    0x0200 // |
#define CMD_AGC       0x0300 // |
#define CMD_BANDWIDTH 0x0400 // |
#define CMD_STEP      0x0500 // |
#define CMD_MODE      0x0600 // |
#define CMD_MENU      0x0700 // |
#define CMD_SOFTMUTEMAXATT 0x0800
#define CMD_AVC       0x0900 //-+
#define CMD_SETTINGS  0x0A00 //-SETTINGS MODE starts here
#define CMD_BRT       0x0B00 // |
#define CMD_CAL       0x0C00 // |
#define CMD_SLEEP     0x0D00 // |
#define CMD_THEME     0x0E00 //-+
#define CMD_ABOUT     0x0F00

//
// Data Types
//

typedef struct
{
  uint8_t idx;      // SI473X device bandwidth index
  const char *desc; // Bandwidth description
} Bandwidth;

typedef struct
{
  int step;         // Step
  const char *desc; // Step description
} Step;

//
// Global Variables
//

extern Band band[];
extern const char *bandModeDesc[];

extern int bandIdx;
extern int8_t bwIdxAM;
extern int8_t bwIdxFM;
extern int8_t bwIdxSSB;

static inline bool isCB()  { return(bandIdx==BAND_CB); }

// These are menu commands
static inline bool isMenuMode(uint16_t cmd)
{
  return((cmd>=CMD_BAND) && (cmd<CMD_SETTINGS));
}

// These are settings
static inline bool isSettingsMode(uint16_t cmd)
{
  return((cmd>=CMD_SETTINGS) && (cmd<CMD_ABOUT));
}

// These are modal commands
static inline bool isModalMode(uint16_t cmd)
{
  return(isMenuMode(cmd) || isSettingsMode(cmd) || (cmd==CMD_ABOUT));
}

void drawSideBar(uint16_t cmd, int x, int y, int sx);
bool doSideBar(uint16_t cmd, int dir);
bool clickSideBar(uint16_t cmd);
void clickVolume();
void selectBand(uint8_t idx, bool drawLoadingSSB = true);
int getTotalBands();
Band *getCurrentBand();
const Step *getCurrentStep();
int getSteps(bool fast = false);
const Bandwidth *getCurrentBandwidth();

void doSoftMute(int dir);
void doAgc(int dir);
void doAvc(int dir);
void doBandwidth(int dir);
void doVolume(int dir);
void doBrt(int dir);
void doStep(int dir);
void doMode(int dir);
void doBand(int dir);

#endif // MENU_H
