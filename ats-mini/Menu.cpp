#include "math.h"
#include "Common.h"
#include "Themes.h"
#include "Utils.h"
#include "Menu.h"
#include "Draw.h"
#include "EIBI.h"

//
// Bands Menu
//
// TO CONFIGURE YOUR OWN BAND PLAN:
// Add new bands by inserting new lines in the table below. Remove
// bands by deleting lines. Change bands by editing lines below.
//
// NOTE:
// You have to RESET EEPROM after adding or removing lines in this
// table. Turn your receiver on with the encoder push button pressed
// at first time to RESET the EEPROM.
//

int bandIdx = 0;

// Band limits are expanded to align with the nearest tuning scale mark
Band bands[] =
{
  {"VHF",  FM_BAND_TYPE, FM,   6400, 10800, 10390, 2, 0, 0},
  // All band. LW, MW and SW (from 150kHz to 30MHz)
  {"ALL",  SW_BAND_TYPE, AM,    150, 30000, 15000, 1, 4, 0},
  {"11M",  SW_BAND_TYPE, AM,  25600, 26100, 25850, 1, 4, 0},
  {"13M",  SW_BAND_TYPE, AM,  21500, 21900, 21650, 1, 4, 0},
  {"15M",  SW_BAND_TYPE, AM,  18900, 19100, 18950, 1, 4, 0},
  {"16M",  SW_BAND_TYPE, AM,  17400, 18100, 17650, 1, 4, 0},
  {"19M",  SW_BAND_TYPE, AM,  15100, 15900, 15450, 1, 4, 0},
  {"22M",  SW_BAND_TYPE, AM,  13500, 13900, 13650, 1, 4, 0},
  {"25M",  SW_BAND_TYPE, AM,  11000, 13000, 11850, 1, 4, 0},
  {"31M",  SW_BAND_TYPE, AM,   9000, 11000,  9650, 1, 4, 0},
  {"41M",  SW_BAND_TYPE, AM,   7000,  9000,  7300, 1, 4, 0},
  {"49M",  SW_BAND_TYPE, AM,   5000,  7000,  6000, 1, 4, 0},
  {"60M",  SW_BAND_TYPE, AM,   4000,  5100,  4950, 1, 4, 0},
  {"75M",  SW_BAND_TYPE, AM,   3500,  4000,  3950, 1, 4, 0},
  {"90M",  SW_BAND_TYPE, AM,   3000,  3500,  3300, 1, 4, 0},
//  {"25M",  SW_BAND_TYPE, AM,  11600, 12100, 11850, 1, 4, 0},
//  {"31M",  SW_BAND_TYPE, AM,   9400,  9900,  9650, 1, 4, 0},
//  {"41M",  SW_BAND_TYPE, AM,   7200,  7500,  7300, 1, 4, 0},
//  {"49M",  SW_BAND_TYPE, AM,   5900,  6200,  6000, 1, 4, 0},
//  {"60M",  SW_BAND_TYPE, AM,   4700,  5100,  4950, 1, 4, 0},
//  {"75M",  SW_BAND_TYPE, AM,   3900,  4000,  3950, 1, 4, 0},
//  {"90M",  SW_BAND_TYPE, AM,   3200,  3400,  3300, 1, 4, 0},
  {"MW3",  MW_BAND_TYPE, AM,   1700,  3500,  2500, 1, 4, 0},
  {"MW2",  MW_BAND_TYPE, AM,    495,  1701,   783, 2, 4, 0},
  {"MW1",  MW_BAND_TYPE, AM,    150,  1800,   810, 3, 4, 0},
  {"160M", MW_BAND_TYPE, LSB,  1800,  2000,  1900, 5, 4, 0},
  {"80M",  SW_BAND_TYPE, LSB,  3500,  4000,  3800, 5, 4, 0},
  {"40M",  SW_BAND_TYPE, LSB,  7000,  7300,  7150, 5, 4, 0},
  {"30M",  SW_BAND_TYPE, LSB, 10000, 10200, 10125, 5, 4, 0},
  {"20M",  SW_BAND_TYPE, USB, 14000, 14400, 14100, 5, 4, 0},
  {"17M",  SW_BAND_TYPE, USB, 18000, 18200, 18115, 5, 4, 0},
  {"15M",  SW_BAND_TYPE, USB, 21000, 21500, 21225, 5, 4, 0},
  {"12M",  SW_BAND_TYPE, USB, 24800, 25000, 24940, 5, 4, 0},
  {"10M",  SW_BAND_TYPE, USB, 28000, 29700, 28500, 5, 4, 0},
  // https://www.hfunderground.com/wiki/CB
  {"CB",   SW_BAND_TYPE, AM,  25000, 30000, 27135, 0, 4, 0},
};

int getTotalBands() { return(ITEM_COUNT(bands)); }
Band *getCurrentBand() { return(&bands[bandIdx]); }

//
// Main Menu
//

#define MENU_MODE         0
#define MENU_BAND         1
#define MENU_VOLUME       2
#define MENU_STEP         3
#define MENU_SEEK         4
#define MENU_MEMORY       5
#define MENU_SQUELCH      6
#define MENU_BW           7
#define MENU_AGC_ATT      8
#define MENU_AVC          9
#define MENU_SOFTMUTE    10
#define MENU_SETTINGS    11
#define MENU_SCAN        12

int8_t menuIdx = MENU_VOLUME;

static const char *menu[] =
{
  "Mode",
  "Band",
  "Volume",
  "Step",
  "Seek",
  "Memory",
  "Squelch",
  "Bandwidth",
  "AGC/ATTN",
  "AVC",
  "SoftMute",
  "Settings",
//  "Scan",
};

//
// Settings Menu
//

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
#define MENU_BLEMODE      14
#define MENU_WIFIMODE     12
#define MENU_ABOUT        13


int8_t settingsIdx = MENU_BRIGHTNESS;

static const char *settings[] =
{
  "Brightness",
  "Calibration",
  "RDS",
  "UTC Offset",
  "FM Region",
  "Theme",
  "UI Layout",
  "Zoom Menu",
  "Scroll Dir.",
  "Sleep",
  "Sleep Mode",
  "Load EiBi",
//  "Bluetooth",
  "Wi-Fi",
  "About",
};

//
// FM Region Menu
//
const FMRegion fmRegions[] = {
  // 50uS de-emphasis
  { 0x1, "EU/JP/AU" },
  // 75uS de-emphasis
  { 0x2, "US" },
};

int getTotalFmRegions() { return(ITEM_COUNT(fmRegions)); }

//
// Mode Menu
//

const char *bandModeDesc[] = { "FM", "LSB", "USB", "AM" };

int getTotalModes() { return(ITEM_COUNT(bandModeDesc)); }

//
// Memory Menu
//

uint8_t memoryIdx = 0;
Memory memories[MEMORY_COUNT];
Memory newMemory;

int getTotalMemories() { return(ITEM_COUNT(memories)); }

//
// RDS Menu
//

uint8_t rdsModeIdx = 0;
static const RDSMode rdsMode[] =
{
  { RDS_PS, "PS"},
  { RDS_PS | RDS_CT, "PS+CT" },
  { RDS_PS | RDS_PI, "PS+PI" },
  { RDS_PS | RDS_PI | RDS_CT, "PS+PI+CT" },
  { RDS_PS | RDS_PI | RDS_RT | RDS_PT, "ALL-CT (EU)" },
  { RDS_PS | RDS_PI | RDS_RT | RDS_PT | RDS_RBDS, "ALL-CT (US)" },
  { RDS_PS | RDS_PI | RDS_RT | RDS_PT | RDS_CT, "ALL (EU)" },
  { RDS_PS | RDS_PI | RDS_RT | RDS_PT | RDS_CT | RDS_RBDS, "ALL (US)" },
};

uint8_t getRDSMode() { return(rdsMode[rdsModeIdx].mode); }

//
// Sleep Mode Menu
//

uint8_t sleepModeIdx = SLEEP_LOCKED;
static const char *sleepModeDesc[] =
{ "Locked", "Unlocked", "CPU Sleep" };

//
// UTC Offset Menu
// FIXME: add more offsets https://en.wikipedia.org/wiki/List_of_UTC_offsets
//
uint8_t utcOffsetIdx = 8;
const UTCOffset utcOffsets[] =
{
  { -8 * 4, "UTC-8", "Fairbanks" },
  { -7 * 4, "UTC-7", "San Francisco" },
  { -6 * 4, "UTC-6", "Denver" },
  { -5 * 4, "UTC-5", "Houston" },
  { -4 * 4, "UTC-4", "New York" },
  { -3 * 4, "UTC-3", "Rio de Janeiro" },
  { -2 * 4, "UTC-2", "Sandwich Islands" },
  { -1 * 4, "UTC-1", "Nuuk" },
  {  0 * 4, "UTC+0", "Reykjavik" },
  {  1 * 4, "UTC+1", "London" },
  {  2 * 4, "UTC+2", "Berlin" },
  {  3 * 4, "UTC+3", "Moscow" },
  {  4 * 4, "UTC+4", "Yerevan" },
  {  5 * 4, "UTC+5", "Astana" },
  {  5 * 4 + 2, "UTC+5:30", "Kolkata" },
  {  6 * 4, "UTC+6", "Omsk" },
  {  7 * 4, "UTC+7", "Novosibirsk" },
  {  8 * 4, "UTC+8", "Beijing" },
  {  9 * 4, "UTC+9", "Yakutsk" },
  { 10 * 4, "UTC+10", "Vladivostok" },
};

int getCurrentUTCOffset() { return(utcOffsets[utcOffsetIdx].offset); }
int getTotalUTCOffsets() { return(ITEM_COUNT(utcOffsets)); }

//
// UI Layout Menu
//
uint8_t uiLayoutIdx = 0;
static const char *uiLayoutDesc[] =
{ "Default", "S-Meter" };

//
// Bluetooth Mode Menu
//

uint8_t bleModeIdx = BLE_OFF;
static const char *bleModeDesc[] =
{ "Off", "DummyEcho" };

int getTotalBleModes() { return(ITEM_COUNT(bleModeDesc)); }

//
// WiFi Mode Menu
//

uint8_t wifiModeIdx = NET_OFF;
static const char *wifiModeDesc[] =
{ "Off", "AP Only", "AP+Connect", "Connect", "Sync Only" };

//
// Step Menu
//

// FM (kHz * 10)
static const Step fmSteps[] =
{
  {   1, "10k",   1 },
  {   5, "50k",   5 },
  {  10, "100k", 10 },
  {  20, "200k", 20 },
  { 100, "1M",   10 },
};

// SSB (Hz)
static const Step ssbSteps[] =
{
  {    10, "10",  1  },
  {    25, "25",  1  },
  {    50, "50",  1  },
  {   100, "100", 1  },
  {   500, "500", 1  },
  {  1000, "1k",  1  },
  {  5000, "5k",  5  },
  {  9000, "9k",  9  },
  { 10000, "10k", 10 },
};

static const uint8_t ssbFastSteps[] =
{
  3, //  10Hz -> 100Hz
  3, //  25Hz -> 100Hz
  4, //  50Hz -> 500Hz
  5, // 100Hz -> 1kHz
  6, // 500Hz -> 5kHz
  6, //  1kHz -> 5kHz
  8, //  5kHz -> 10kHz
  7, //  9kHz -> 9kHz
  8, // 10kHz -> 10kHz
};

// AM (kHz)
static const Step amSteps[] =
{
  {    1, "1k",    1 },
  {    5, "5k",    5 },
  {    9, "9k",    9 },
  {   10, "10k",  10 },
  {   50, "50k",  10 },
  {  100, "100k", 10 },
  { 1000, "1M",   10 },
};

static const Step *steps[4] = { fmSteps, ssbSteps, ssbSteps, amSteps };
static const uint8_t defaultStepIdx[4] = { 2, 5, 5, 1 };

static int getLastStep(int mode)
{
  switch(mode)
  {
    case FM:  return(LAST_ITEM(fmSteps));
    case LSB: return(LAST_ITEM(ssbSteps));
    case USB: return(LAST_ITEM(ssbSteps));
    case AM:  return(LAST_ITEM(amSteps));
  }

  return(0);
}

const Step *getCurrentStep(bool fast)
{
  uint8_t idx = bands[bandIdx].currentStepIdx > getLastStep(currentMode) ? defaultStepIdx[currentMode] : bands[bandIdx].currentStepIdx;
  return(&steps[currentMode][fast && isSSB()? ssbFastSteps[idx]:idx]);
}

static uint8_t freqInputPos = 0;

static uint8_t getDefaultFreqInputPos(int mode, int step)
{
  return (uint8_t)(log10(step) * 2) + (mode == AM ? 6 : 0);
}

void resetFreqInputPos()
{
  freqInputPos = getDefaultFreqInputPos(currentMode, getCurrentStep(false)->step);
}

uint8_t getFreqInputPos()
{
  return freqInputPos;
}

int getFreqInputStep()
{
  return freqInputPos % 2 ?
    5 * pow(10, (freqInputPos - (currentMode == AM ? 6 : 0) - 1) / 2) :
            pow(10, (freqInputPos - (currentMode == AM ? 6 : 0)) / 2);
}

static uint8_t getMinFreqInputPos()
{
  return getDefaultFreqInputPos(currentMode, steps[currentMode][0].step);
}

static uint8_t getMaxFreqInputPos()
{
  return (uint8_t)log10(getCurrentBand()->maximumFreq) * 2 + (currentMode != FM ? 6 : -2);
}

//
// Bandwidth Menu
//

static const Bandwidth fmBandwidths[] =
{
  { 0, "Auto" }, // Automatic - default
  { 1, "110k" }, // Force wide (110 kHz) channel filter.
  { 2, "84k"  },
  { 3, "60k"  },
  { 4, "40k"  }
};

static const Bandwidth ssbBandwidths[] =
{
  { 4, "0.5k" },
  { 5, "1.0k" },
  { 0, "1.2k" },
  { 1, "2.2k" },
  { 2, "3.0k" },
  { 3, "4.0k" }
};

static const Bandwidth amBandwidths[] =
{
  { 4, "1.0k" },
  { 5, "1.8k" },
  { 3, "2.0k" },
  { 6, "2.5k" },
  { 2, "3.0k" },
  { 1, "4.0k" },
  { 0, "6.0k" }
};

static const Bandwidth *bandwidths[4] =
{
  fmBandwidths, ssbBandwidths, ssbBandwidths, amBandwidths
};

static const uint8_t defaultBwIdx[4] = { 0, 4, 4, 4 };

static int getLastBandwidth(int mode)
{
  switch(mode)
  {
    case FM:  return(LAST_ITEM(fmBandwidths));
    case LSB: return(LAST_ITEM(ssbBandwidths));
    case USB: return(LAST_ITEM(ssbBandwidths));
    case AM:  return(LAST_ITEM(amBandwidths));
  }

  return(0);
}

const Bandwidth *getCurrentBandwidth()
{
  return(&bandwidths[currentMode][bands[bandIdx].bandwidthIdx > getLastBandwidth(currentMode) ? defaultBwIdx[currentMode] : bands[bandIdx].bandwidthIdx]);
}

static void setBandwidth()
{
  uint8_t idx = getCurrentBandwidth()->idx;

  switch(currentMode)
  {
    case FM:
      rx.setFmBandwidth(idx);
      break;
    case AM:
      rx.setBandwidth(idx, 1);
      break;
    case LSB:
    case USB:
      // Set Audio
      rx.setSSBAudioBandwidth(idx);
      // If audio bandwidth selected is about 2 kHz or below, it is
      // recommended to set Sideband Cutoff Filter to 0.
      rx.setSSBSidebandCutoffFilter(idx==0 || idx==4 || idx==5? 0 : 1);
      break;
  }
}

// Seek mode. Pass true to toggle, false to return the current one
uint8_t seekMode(bool toggle)
{
  static uint8_t mode = SEEK_DEFAULT;

  mode = toggle ? (mode == SEEK_DEFAULT ? SEEK_SCHEDULE : SEEK_DEFAULT) : mode;

  // Use normal seek on FM or if there is no schedule loaded
  if(currentMode == FM || !eibiAvailable() || !clockAvailable())
    return(SEEK_DEFAULT);

  return(mode);
}

//
// Utility functions to change menu values
//

static inline int min(int x, int y) { return(x<y? x:y); }

static inline int wrap_range(int v, int dir, int vMin, int vMax)
{
  v += dir;
  v  = v>vMax? vMin + (v - vMax - 1) : v<vMin? vMax - (vMin - v - 1) : v;
  return(v);
}

static inline int clamp_range(int v, int dir, int vMin, int vMax)
{
  v += dir;
  v  = v>vMax? vMax : v<vMin? vMin : v;
  return(v);
}

//
// Encoder input handlers
//

void doSelectDigit(int dir)
{
  freqInputPos = clamp_range(freqInputPos, -dir, getMinFreqInputPos(), getMaxFreqInputPos());
}

void doVolume(int dir)
{
  volume = clamp_range(volume, dir, 0, 63);
  if(!muteOn()) rx.setVolume(volume);
}

static void clickVolume(bool shortPress)
{
  if(shortPress) muteOn(!muteOn()); else currentCmd = CMD_NONE;
}

static void clickSquelch(bool shortPress)
{
  if(shortPress) currentSquelch = 0; else currentCmd = CMD_NONE;
}

static void clickSeek(bool shortPress)
{
  if(shortPress) seekMode(true); else currentCmd = CMD_NONE;
}

static void doTheme(int dir)
{
  themeIdx = wrap_range(themeIdx, dir, 0, getTotalThemes() - 1);
}

static void doUILayout(int dir)
{
  uiLayoutIdx = uiLayoutIdx > LAST_ITEM(uiLayoutDesc) ? UI_DEFAULT : wrap_range(uiLayoutIdx, dir, 0, LAST_ITEM(uiLayoutDesc));
}

void doAvc(int dir)
{
  // Only allow for AM and SSB modes
  if(currentMode==FM) return;

  if(isSSB())
  {
    SsbAvcIdx = wrap_range(SsbAvcIdx, 2*dir, 12, 90);
    rx.setAvcAmMaxGain(SsbAvcIdx);
  }
  else
  {
    AmAvcIdx = wrap_range(AmAvcIdx, 2*dir, 12, 90);
    rx.setAvcAmMaxGain(AmAvcIdx);
  }
}

void doFmRegion(int dir)
{
  // Only allow for FM mode
  if(currentMode!=FM) return;

  FmRegionIdx = wrap_range(FmRegionIdx, dir, 0, LAST_ITEM(fmRegions));
  rx.setFMDeEmphasis(fmRegions[FmRegionIdx].value);
}

void doCal(int dir)
{
  if (currentMode == USB)
    bands[bandIdx].usbCal = clamp_range(bands[bandIdx].usbCal, 10*dir, -MAX_CAL, MAX_CAL);
  else if (currentMode == LSB)
    bands[bandIdx].lsbCal = clamp_range(bands[bandIdx].lsbCal, 10*dir, -MAX_CAL, MAX_CAL);
  // else: no calibration change for other modes

  // If in SSB mode set the SI4732/5 BFO value
  // This adjusts the BFO while in the calibration menu
  if(isSSB()) updateBFO(currentBFO, true);
}

void doBrt(int dir)
{
  currentBrt = clamp_range(currentBrt, 5*dir, 10, 255);
  if(!sleepOn()) ledcWrite(PIN_LCD_BL, currentBrt);
}

static void doSleep(int dir)
{
  currentSleep = clamp_range(currentSleep, 5*dir, 0, 255);
}

static void doSleepMode(int dir)
{
  sleepModeIdx = wrap_range(sleepModeIdx, dir, 0, LAST_ITEM(sleepModeDesc));
}

static void doBleMode(int dir)
{
  uint8_t newBleModeIdx = wrap_range(bleModeIdx, dir, 0, LAST_ITEM(bleModeDesc));
  bleInit(newBleModeIdx);
  bleModeIdx = newBleModeIdx;
}

static void doWiFiMode(int dir)
{
  wifiModeIdx = wrap_range(wifiModeIdx, dir, 0, LAST_ITEM(wifiModeDesc));
}

static void clickWiFiMode(uint8_t mode, bool shortPress)
{
  currentCmd = CMD_NONE;
  netInit(mode);
}

static void doRDSMode(int dir)
{
  rdsModeIdx = wrap_range(rdsModeIdx, dir, 0, LAST_ITEM(rdsMode));
  if(!(getRDSMode() & RDS_CT)) clockReset();
}

static void doUTCOffset(int dir)
{
  utcOffsetIdx = wrap_range(utcOffsetIdx, dir, 0, LAST_ITEM(utcOffsets));
  clockRefreshTime();
}

static void doZoom(int dir)
{
  zoomMenu = !zoomMenu;
}

static void doScrollDir(int dir)
{
  scrollDirection = (scrollDirection == 1) ? -1 : 1;
}

uint8_t doAbout(int dir)
{
  static uint8_t aboutScreen = 0;
  aboutScreen = clamp_range(aboutScreen, dir, 0, 2);
  return aboutScreen;
}

bool tuneToMemory(const Memory *memory)
{
  // Must have frequency
  if(!memory->freq) return(false);

  // Must have valid band index
  if(memory->band>=getTotalBands()) return(false);

  // Band must contain frequency and modulation
  if(!isMemoryInBand(&bands[memory->band], memory)) return(false);

  // Must differ from the current band, frequency and modulation
  if(memory->band==bandIdx &&
     memory->freq==bands[bandIdx].currentFreq &&
     memory->mode==bands[bandIdx].bandMode)
    return(true);

  // Save current band settings
  bands[bandIdx].currentFreq    = currentFrequency + currentBFO / 1000;

  // Load frequency and modulation from memory slot
  bands[memory->band].currentFreq = memory->freq;
  bands[memory->band].bandMode    = memory->mode;

  // Enable the new band
  selectBand(memory->band);

  // Update BFO if present in memory slot
  if(memory->hz100) updateBFO(memory->hz100 * 100);

  return(true);
}

static void doMemory(int dir)
{
  memoryIdx = wrap_range(memoryIdx, dir, 0, LAST_ITEM(memories));
  if(!tuneToMemory(&memories[memoryIdx])) tuneToMemory(&newMemory);
}

static void clickMemory(uint8_t idx, bool shortPress)
{
  // Must have a valid index
  if(idx>LAST_ITEM(memories)) return;

  // If clicking on an empty memory slot, save to it
  if(!memories[idx].freq) memories[idx] = newMemory;
  // On a press, delete memory slot contents
  else if(shortPress) memories[idx].freq = 0;
  // On a click, do nothing, slot already activated in doMemory()
  else currentCmd = CMD_NONE;
}

void doStep(int dir)
{
  uint8_t idx = bands[bandIdx].currentStepIdx;

  idx = wrap_range(idx, dir, 0, getLastStep(currentMode));
  bands[bandIdx].currentStepIdx = idx;

  rx.setFrequencyStep(steps[currentMode][idx].step);

  // Set seek spacing
  if(currentMode==FM)
    rx.setSeekFmSpacing(steps[currentMode][idx].spacing);
  else
    rx.setSeekAmSpacing(steps[currentMode][idx].spacing);
}

void doAgc(int dir)
{
  if(currentMode==FM)
    agcIdx = FmAgcIdx = wrap_range(FmAgcIdx, dir, 0, 27);
  else if(isSSB())
    agcIdx = SsbAgcIdx = wrap_range(SsbAgcIdx, dir, 0, 1);
  else
    agcIdx = AmAgcIdx = wrap_range(AmAgcIdx, dir, 0, 37);

  // Process agcIdx to generate disableAgc and agcIdx
  // agcIdx     0 1 2 3 4 5 6  ..... n    (n:    FM = 27, AM = 37, SSB = 1)
  // agcNdx     0 0 1 2 3 4 5  ..... n -1 (n -1: FM = 26, AM = 36, SSB = 0)
  // disableAgc 0 1 1 1 1 1 1  ..... 1

  // if true, disable AGC; else, AGC is enabled
  disableAgc = agcIdx>0? 1 : 0;
  agcNdx     = agcIdx>1? agcIdx - 1 : 0;

  // Configure SI4732/5 (if agcNdx = 0, no attenuation)
  rx.setAutomaticGainControl(disableAgc, agcNdx);
}

void doMode(int dir)
{
  // This is our current mode for the current band
  currentMode = bands[bandIdx].bandMode;

  // Cannot change away from FM mode
  if(currentMode==FM) return;

  // Change AM/LSB/USB modes, do not allow FM mode
  do
    currentMode = wrap_range(currentMode, dir, 0, LAST_ITEM(bandModeDesc));
  while(currentMode==FM);

  // Save current band settings
  bands[bandIdx].currentFreq = currentFrequency + currentBFO / 1000;
  bands[bandIdx].currentStepIdx = defaultStepIdx[currentMode];
  bands[bandIdx].bandwidthIdx = defaultBwIdx[currentMode];
  bands[bandIdx].bandMode = currentMode;

  // Enable the new band
  selectBand(bandIdx);
}

void doSquelch(int dir)
{
  currentSquelch = clamp_range(currentSquelch, dir, 0, 127);
}

void doSoftMute(int dir)
{
  // Nothing to do if FM mode
  if(currentMode==FM) return;

  if(isSSB())
    softMuteMaxAttIdx = SsbSoftMuteIdx = wrap_range(SsbSoftMuteIdx, dir, 0, 32);
  else
    softMuteMaxAttIdx = AmSoftMuteIdx = wrap_range(AmSoftMuteIdx, dir, 0, 32);

  rx.setAmSoftMuteMaxAttenuation(softMuteMaxAttIdx);
}

void doBand(int dir)
{
  // Save current band settings
  bands[bandIdx].currentFreq = currentFrequency + currentBFO / 1000;
  bands[bandIdx].bandMode = currentMode;

  // Change band
  bandIdx = wrap_range(bandIdx, dir, 0, LAST_ITEM(bands));

  // Enable the new band
  selectBand(bandIdx);
}

void doBandwidth(int dir)
{
  uint8_t idx = bands[bandIdx].bandwidthIdx;

  idx = wrap_range(idx, dir, 0, getLastBandwidth(currentMode));
  bands[bandIdx].bandwidthIdx = idx;
  setBandwidth();
}

//
// Handle encoder input in menu
//

static void doMenu(int dir)
{
  menuIdx = wrap_range(menuIdx, dir, 0, LAST_ITEM(menu));
}

static void clickMenu(int cmd, bool shortPress)
{
  // No command yet
  currentCmd = CMD_NONE;

  switch(cmd)
  {
    case MENU_STEP:     currentCmd = CMD_STEP;      break;
    case MENU_SEEK:     currentCmd = CMD_SEEK;      break;
    case MENU_MODE:     currentCmd = CMD_MODE;      break;
    case MENU_BW:       currentCmd = CMD_BANDWIDTH; break;
    case MENU_AGC_ATT:  currentCmd = CMD_AGC;       break;
    case MENU_BAND:     currentCmd = CMD_BAND;      break;
    case MENU_SETTINGS: currentCmd = CMD_SETTINGS;  break;
    case MENU_SQUELCH:  currentCmd = CMD_SQUELCH;   break;
    case MENU_VOLUME:   currentCmd = CMD_VOLUME;    break;

    case MENU_MEMORY:
      currentCmd = CMD_MEMORY;
      newMemory.freq  = currentFrequency + currentBFO / 1000;
      newMemory.hz100 = (currentBFO % 1000) / 100;
      newMemory.mode  = currentMode;
      newMemory.band  = bandIdx;
      doMemory(0);
      break;

    case MENU_SOFTMUTE:
      // No soft mute in FM mode
      if(currentMode!=FM) currentCmd = CMD_SOFTMUTE;
      break;

    case MENU_AVC:
      // No AVC in FM mode
      if(currentMode!=FM) currentCmd = CMD_AVC;
      break;

    case MENU_SCAN:
      // Run a band scan around current frequency with the same
      // step as scale resolution (10kHz for AM, 100kHz for FM)
      drawMessage("Scanning...");
      scanRun(currentFrequency, 10);
      break;
  }
}

static void doSettings(int dir)
{
  settingsIdx = wrap_range(settingsIdx, dir, 0, LAST_ITEM(settings));
}

static void clickSettings(int cmd, bool shortPress)
{
  // No command yet
  currentCmd = CMD_NONE;

  switch(cmd)
  {
    case MENU_BRIGHTNESS: currentCmd = CMD_BRT; break;
    case MENU_CALIBRATION:
      if(isSSB()) currentCmd = CMD_CAL;
      break;
    case MENU_THEME:      currentCmd = CMD_THEME;     break;
    case MENU_UI:         currentCmd = CMD_UI;        break;
    case MENU_RDS:        currentCmd = CMD_RDS;       break;
    case MENU_ZOOM:       currentCmd = CMD_ZOOM;      break;
    case MENU_SCROLL:     currentCmd = CMD_SCROLL;    break;
    case MENU_SLEEP:      currentCmd = CMD_SLEEP;     break;
    case MENU_SLEEPMODE:  currentCmd = CMD_SLEEPMODE; break;
    case MENU_UTCOFFSET:  currentCmd = CMD_UTCOFFSET; break;
    case MENU_BLEMODE:    currentCmd = CMD_BLEMODE;   break;
    case MENU_WIFIMODE:   currentCmd = CMD_WIFIMODE;  break;
    case MENU_FM_REGION:
      // Only in FM mode
      if(currentMode==FM) currentCmd = CMD_FM_REGION;
      break;
    case MENU_ABOUT:      currentCmd = CMD_ABOUT;     break;

    case MENU_LOADEIBI:
      eibiLoadSchedule();
      break;
  }
}

bool doSideBar(uint16_t cmd, int dir)
{
  // Ignore idle encoder
  if(!dir) return(false);

  switch(cmd)
  {
    // Menus and list-based options must take scrollDirection into account
    case CMD_MENU:      doMenu(scrollDirection * dir);break;
    case CMD_MODE:      doMode(scrollDirection * dir);break;
    case CMD_STEP:      doStep(scrollDirection * dir);break;
    case CMD_AGC:       doAgc(dir);break;
    case CMD_BANDWIDTH: doBandwidth(scrollDirection * dir);break;
    case CMD_VOLUME:    doVolume(dir);break;
    case CMD_SOFTMUTE:  doSoftMute(dir);break;
    case CMD_BAND:      doBand(scrollDirection * dir);break;
    case CMD_AVC:       doAvc(dir);break;
    case CMD_FM_REGION: doFmRegion(scrollDirection * dir);break;
    case CMD_SETTINGS:  doSettings(scrollDirection * dir);break;
    case CMD_BRT:       doBrt(dir);break;
    case CMD_CAL:       doCal(dir);break;
    case CMD_THEME:     doTheme(scrollDirection * dir);break;
    case CMD_UI:        doUILayout(scrollDirection * dir);break;
    case CMD_RDS:       doRDSMode(scrollDirection * dir);break;
    case CMD_MEMORY:    doMemory(scrollDirection * dir);break;
    case CMD_SLEEP:     doSleep(dir);break;
    case CMD_SLEEPMODE: doSleepMode(scrollDirection * dir);break;
    case CMD_BLEMODE:   doBleMode(scrollDirection * dir);break;
    case CMD_WIFIMODE:  doWiFiMode(scrollDirection * dir);break;
    case CMD_ZOOM:      doZoom(dir);break;
    case CMD_SCROLL:    doScrollDir(dir);break;
    case CMD_UTCOFFSET: doUTCOffset(scrollDirection * dir);break;
    case CMD_SQUELCH:   doSquelch(dir);break;
    case CMD_ABOUT:     doAbout(dir);break;
    default:            return(false);
  }

  // Encoder input handled
  return(true);
}

bool clickHandler(uint16_t cmd, bool shortPress)
{
  switch(cmd)
  {
    case CMD_MENU:     clickMenu(menuIdx, shortPress);break;
    case CMD_SETTINGS: clickSettings(settingsIdx, shortPress);break;
    case CMD_MEMORY:   clickMemory(memoryIdx, shortPress);break;
    case CMD_WIFIMODE: clickWiFiMode(wifiModeIdx, shortPress);break;
    case CMD_VOLUME:   clickVolume(shortPress);break;
    case CMD_SQUELCH:  clickSquelch(shortPress);break;
    case CMD_SEEK:     clickSeek(shortPress);break;
    case CMD_FREQ:     return(clickFreq(shortPress));
    default:           return(false);
  }

  // Encoder input handled
  return(true);
}

//
// Selecting given band
//

void selectBand(uint8_t idx, bool drawLoadingSSB)
{
  // Silence click on some hardware versions
  // https://github.com/esp32-si4732/ats-mini/discussions/103
  tempMuteOn(true);

  // Set band and mode
  bandIdx = min(idx, LAST_ITEM(bands));
  currentMode = bands[bandIdx].bandMode;

  // Load SSB patch as needed
  if(isSSB())
    loadSSB(getCurrentBandwidth()->idx, drawLoadingSSB);
  else
    unloadSSB();

  // Switch radio to the selected band
  useBand(&bands[bandIdx]);

  // Set bandwidth for the current mode
  setBandwidth();

  // Clear current station info (RDS/CB)
  clearStationInfo();

  // Check for named frequencies
  identifyFrequency(currentFrequency + currentBFO / 1000);

  // Set default digit position based on the current step
  resetFreqInputPos();

  // Unmute the sound
  tempMuteOn(false);
}

//
// Draw functions
//

static void drawCommon(const char *title, int x, int y, int sx, bool cursor = false)
{
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_hdr, TH.menu_bg);
  spr.fillSmoothRoundRect(1+x, 1+y, 76+sx, 110, 4, TH.menu_border);
  spr.fillSmoothRoundRect(2+x, 2+y, 74+sx, 108, 4, TH.menu_bg);

  spr.drawString(title, 40+x+(sx/2), 12+y, 2);
  spr.drawLine(1+x, 23+y, 76+sx, 23+y, TH.menu_border);

  spr.setTextFont(0);
  spr.setTextColor(TH.menu_item, TH.menu_bg);
  if(cursor)
    spr.fillRoundRect(6+x, 24+y+(2*16), 66+sx, 16, 2, TH.menu_hl_bg);
}

static void drawMenu(int x, int y, int sx)
{
  spr.setTextDatum(MC_DATUM);

  spr.fillSmoothRoundRect(1+x, 1+y, 76+sx, 110, 4, TH.menu_border);
  spr.fillSmoothRoundRect(2+x, 2+y, 74+sx, 108, 4, TH.menu_bg);
  spr.setTextColor(TH.menu_hdr, TH.menu_bg);

  spr.drawString("Menu", 40+x+(sx/2), 12+y, 2);
  spr.drawLine(1+x, 23+y, 76+sx, 23+y, TH.menu_border);

  spr.setTextFont(0);
  spr.setTextColor(TH.menu_item, TH.menu_bg);
  spr.fillRoundRect(6+x, 24+y+(2*16), 66+sx, 16, 2, TH.menu_hl_bg);

  int count = ITEM_COUNT(menu);
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(menu[abs((menuIdx+count+i)%count)]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item, TH.menu_bg);
    }
    spr.setTextDatum(MC_DATUM);
    spr.drawString(menu[abs((menuIdx+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawSettings(int x, int y, int sx)
{
  spr.setTextDatum(MC_DATUM);

  spr.fillSmoothRoundRect(1+x, 1+y, 76+sx, 110, 4, TH.menu_border);
  spr.fillSmoothRoundRect(2+x, 2+y, 74+sx, 108, 4, TH.menu_bg);
  spr.setTextColor(TH.menu_hdr, TH.menu_bg);
  spr.drawString("Settings", 40+x+(sx/2), 12+y, 2);
  spr.drawLine(1+x, 23+y, 76+sx, 23+y, TH.menu_border);

  spr.setTextFont(0);
  spr.setTextColor(TH.menu_item, TH.menu_bg);
  spr.fillRoundRect(6+x, 24+y+(2*16), 66+sx, 16, 2, TH.menu_hl_bg);

  int count = ITEM_COUNT(settings);
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(settings[abs((settingsIdx+count+i)%count)]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item, TH.menu_bg);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(settings[abs((settingsIdx+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawMode(int x, int y, int sx)
{
  drawCommon(menu[MENU_MODE], x, y, sx, true);

  int count = ITEM_COUNT(bandModeDesc);
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(bandModeDesc[abs((currentMode+count+i)%count)]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item, TH.menu_bg);
    }

    spr.setTextDatum(MC_DATUM);
    if((currentMode!=FM) || (i==0))
     spr.drawString(bandModeDesc[abs((currentMode+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawStep(int x, int y, int sx)
{
  int count = getLastStep(currentMode) + 1;
  int idx   = bands[bandIdx].currentStepIdx + count;

  drawCommon(menu[MENU_STEP], x, y, sx, true);

  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(steps[currentMode][abs((idx+i)%count)].desc);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item, TH.menu_bg);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(steps[currentMode][abs((idx+i)%count)].desc, 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawSeek(int x, int y, int sx)
{
  drawCommon(menu[MENU_SEEK], x, y, sx);
  spr.drawSmoothArc(40+x+(sx/2), 66+y, 30, 27, 45, 180, TH.menu_param, TH.menu_bg);
  spr.fillTriangle(40+x+(sx/2)-5, 66+y-32, 40+x+(sx/2)+5, 66+y-27, 40+x+(sx/2)-5, 66+y-22, TH.menu_param);
  spr.drawSmoothArc(40+x+(sx/2), 66+y, 30, 27, 225, 360, TH.menu_param, TH.menu_bg);
  spr.fillTriangle(40+x+(sx/2)+5, 66+y+32, 40+x+(sx/2)-5, 66+y+27, 40+x+(sx/2)+5, 66+y+22, TH.menu_param);

  if(seekMode()==SEEK_SCHEDULE)
  {
    spr.drawCircle(40+x+(sx/2), 66+y, 10, TH.menu_param);
    spr.drawLine(40+x+(sx/2), 66+y, 40+x+(sx/2), 66+y-7, TH.menu_param);
    spr.drawLine(40+x+(sx/2), 66+y, 40+x+(sx/2)+4, 66+y+4, TH.menu_param);
  }
}

static void drawBand(int x, int y, int sx)
{
  drawCommon(menu[MENU_BAND], x, y, sx, true);

  int count = ITEM_COUNT(bands);
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(bands[abs((bandIdx+count+i)%count)].bandName);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item, TH.menu_bg);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(bands[abs((bandIdx+count+i)%count)].bandName, 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawBandwidth(int x, int y, int sx)
{
  int count = getLastBandwidth(currentMode) + 1;
  int idx   = bands[bandIdx].bandwidthIdx + count;

  drawCommon(menu[MENU_BW], x, y, sx, true);

  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(bandwidths[currentMode][abs((idx+i)%count)].desc);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item, TH.menu_bg);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(bandwidths[currentMode][abs((idx+i)%count)].desc, 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawSleepMode(int x, int y, int sx)
{
  drawCommon(settings[MENU_SLEEPMODE], x, y, sx, true);

  int count = ITEM_COUNT(sleepModeDesc);
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(sleepModeDesc[abs((sleepModeIdx+count+i)%count)]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item, TH.menu_bg);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(sleepModeDesc[abs((sleepModeIdx+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawBleMode(int x, int y, int sx)
{
  drawCommon(settings[MENU_BLEMODE], x, y, sx, true);

  int count = ITEM_COUNT(bleModeDesc);
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(bleModeDesc[abs((bleModeIdx+count+i)%count)]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item, TH.menu_bg);
    }

    // Prevent repeats for short menus
    if (count < 5 && ((bleModeIdx+i) < 0 || (bleModeIdx+i) >= count)) {
      continue;
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(bleModeDesc[abs((bleModeIdx+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawWiFiMode(int x, int y, int sx)
{
  drawCommon(settings[MENU_WIFIMODE], x, y, sx, true);

  int count = ITEM_COUNT(wifiModeDesc);
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(wifiModeDesc[abs((wifiModeIdx+count+i)%count)]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item, TH.menu_bg);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(wifiModeDesc[abs((wifiModeIdx+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawTheme(int x, int y, int sx)
{
  drawCommon(settings[MENU_THEME], x, y, sx, true);

  int count = getTotalThemes();
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(theme[abs((themeIdx+count+i)%count)].name);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item, TH.menu_bg);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(theme[abs((themeIdx+count+i)%count)].name, 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawUILayout(int x, int y, int sx)
{
  drawCommon(settings[MENU_UI], x, y, sx, true);

  int count = ITEM_COUNT(uiLayoutDesc);
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(uiLayoutDesc[abs((uiLayoutIdx+count+i)%count)]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item, TH.menu_bg);
    }

    // Prevent repeats for short menus
    if (count < 5 && ((uiLayoutIdx+i) < 0 || (uiLayoutIdx+i) >= count)) {
      continue;
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(uiLayoutDesc[abs((uiLayoutIdx+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawRDSMode(int x, int y, int sx)
{
  drawCommon(settings[MENU_RDS], x, y, sx, true);

  int count = ITEM_COUNT(rdsMode);
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(rdsMode[abs((rdsModeIdx+count+i)%count)].desc);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item, TH.menu_bg);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(rdsMode[abs((rdsModeIdx+count+i)%count)].desc, 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawUTCOffset(int x, int y, int sx)
{
  drawCommon(settings[MENU_UTCOFFSET], x, y, sx, true);

  int count = ITEM_COUNT(utcOffsets);
  uint8_t idx = utcOffsetIdx;

  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0)
    {
      drawZoomedMenu(utcOffsets[abs((idx+count+i)%count)].desc);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    }
    else
    {
      spr.setTextColor(TH.menu_item, TH.menu_bg);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(utcOffsets[abs((idx+count+i)%count)].desc, 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawMemory(int x, int y, int sx)
{
  char label_memory[16];
  sprintf(label_memory, "%s %2.2d", menu[MENU_MEMORY], memoryIdx + 1);
  drawCommon(label_memory, x, y, sx, true);

  int count = ITEM_COUNT(memories);
  for(int i=-2 ; i<3 ; i++)
  {
    int j = abs((memoryIdx+count+i)%count);
    char buf[16];
    const char *text = buf;

    if(i==0 && !memories[j].freq)
      text = "Add";
    else if(!memories[j].freq)
      text = "- - -";
    else if(memories[j].mode==FM)
      sprintf(buf, "%3.2f %s", memories[j].freq / 100.0, bandModeDesc[memories[j].mode]);
    else
      sprintf(buf, "%5d %s", memories[j].freq, bandModeDesc[memories[j].mode]);

    if(i==0) {
      drawZoomedMenu(text);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item, TH.menu_bg);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(text, 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawVolume(int x, int y, int sx)
{
  drawCommon(menu[MENU_VOLUME], x, y, sx);
  drawZoomedMenu(menu[MENU_VOLUME]);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_param, TH.menu_bg);
  spr.drawNumber(volume, 40+x+(sx/2), 66+y, 7);

  if(muteOn())
  {
    for(int i=-3; i<4; i++)
    {
      spr.drawLine(40+x+(sx/2) + 30 + i, 66 + y - 30 + i, 40+x+(sx/2) - 30 + i, 66 + y + 30 + i, TH.menu_param);
    }
  }
}

static void drawAgc(int x, int y, int sx)
{
  drawCommon(menu[MENU_AGC_ATT], x, y, sx);
  drawZoomedMenu(menu[MENU_AGC_ATT]);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TH.menu_param, TH.menu_bg);

  // G8PTN: Read back value is not used
  // rx.getAutomaticGainControl();
  if(!agcNdx && !agcIdx)
  {
    spr.setFreeFont(&Orbitron_Light_24);
    spr.drawString("AGC", 40+x+(sx/2), 48+y);
    spr.drawString("On", 40+x+(sx/2), 72+y);
    spr.setTextFont(0);
  }
  else
  {
    char text[16];
    sprintf(text, "%2.2d", agcNdx);
    spr.drawString(text, 40+x+(sx/2), 60+y, 7);
  }
}

static void drawSquelch(int x, int y, int sx)
{
  drawCommon(menu[MENU_SQUELCH], x, y, sx);
  drawZoomedMenu(menu[MENU_SQUELCH]);
  spr.setTextDatum(MC_DATUM);

  if(currentSquelch)
  {
    spr.drawNumber(currentSquelch, 40+x+(sx/2), 60+y, 4);
    spr.drawString("dBuV", 40+x+(sx/2), 90+y, 4);
  }
  else
  {
    spr.drawString("Off", 40+x+(sx/2), 60+y, 4);
  }
}

static void drawSoftMuteMaxAtt(int x, int y, int sx)
{
  drawCommon(menu[MENU_SOFTMUTE], x, y, sx);
  drawZoomedMenu(menu[MENU_SOFTMUTE]);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_param, TH.menu_bg);
  spr.drawString("Max Attn", 40+x+(sx/2), 32+y, 2);
  spr.drawNumber(softMuteMaxAttIdx, 40+x+(sx/2), 60+y, 4);
  spr.drawString("dB", 40+x+(sx/2), 90+y, 4);
}

static void drawCal(int x, int y, int sx)
{
  drawCommon(settings[MENU_CALIBRATION], x, y, sx);
  drawZoomedMenu(settings[MENU_CALIBRATION]);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_param, TH.menu_bg);
  if (currentMode == USB)
    spr.drawNumber(getCurrentBand()->usbCal, 40+x+(sx/2), 60+y, 4);
  else if (currentMode == LSB)
    spr.drawNumber(getCurrentBand()->lsbCal, 40+x+(sx/2), 60+y, 4);
  else
    spr.drawNumber(0, 40+x+(sx/2), 60+y, 4);  // Display zero or nothing for other modes

  spr.drawString("Hz", 40+x+(sx/2), 90+y, 4);
}

static void drawAvc(int x, int y, int sx)
{
  drawCommon(menu[MENU_AVC], x, y, sx);
  drawZoomedMenu(menu[MENU_AVC]);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_param, TH.menu_bg);
  spr.drawString("Max Gain", 40+x+(sx/2), 32+y, 2);

  // Only show AVC for AM and SSB modes
  if(currentMode!=FM)
  {
    int currentAvc = isSSB()? SsbAvcIdx : AmAvcIdx;
    spr.drawNumber(currentAvc, 40+x+(sx/2), 60+y, 4);
    spr.drawString("dB", 40+x+(sx/2), 90+y, 4);
  }
}

static void drawFmRegion(int x, int y, int sx)
{
  drawCommon(settings[MENU_FM_REGION], x, y, sx, true);

  int count = ITEM_COUNT(fmRegions);
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(fmRegions[abs((FmRegionIdx+count+i)%count)].desc);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item, TH.menu_bg);
    }

    // Prevent repeats for short menus
    if (count < 5 && ((FmRegionIdx+i) < 0 || (FmRegionIdx+i) >= count)) {
      continue;
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(fmRegions[abs((FmRegionIdx+count+i)%count)].desc, 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawBrt(int x, int y, int sx)
{
  drawCommon(settings[MENU_BRIGHTNESS], x, y, sx);
  drawZoomedMenu(settings[MENU_BRIGHTNESS]);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_param, TH.menu_bg);
  spr.drawNumber(currentBrt, 40+x+(sx/2), 60+y, 4);
}

static void drawSleep(int x, int y, int sx)
{
  drawCommon(settings[MENU_SLEEP], x, y, sx);
  drawZoomedMenu(settings[MENU_SLEEP]);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_param, TH.menu_bg);
  spr.drawNumber(currentSleep, 40+x+(sx/2), 60+y, 4);
}

static void drawZoom(int x, int y, int sx)
{
  drawCommon(settings[MENU_ZOOM], x, y, sx);
  drawZoomedMenu(settings[MENU_ZOOM]);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_param, TH.menu_bg);
  spr.drawString(zoomMenu ? "On" : "Off", 40+x+(sx/2), 60+y, 4);
}

static void drawScrollDir(int x, int y, int sx)
{
  drawCommon(settings[MENU_SCROLL], x, y, sx);
  drawZoomedMenu(settings[MENU_SCROLL]);

  spr.fillRect(37+x+(sx/2), 45+y, 5, 40, TH.menu_param);
  if(scrollDirection>0)
    spr.fillTriangle(39+x+(sx/2)-5, 45+y, 39+x+(sx/2)+5, 45+y, 39+x+(sx/2), 45+y-5, TH.menu_param);
  else
    spr.fillTriangle(39+x+(sx/2)-5, 85+y, 39+x+(sx/2)+5, 85+y, 39+x+(sx/2), 85+y+5, TH.menu_param);
}

static void drawInfo(int x, int y, int sx)
{
  char text[16];

  // Info box
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(TH.box_text, TH.box_bg);
  spr.fillSmoothRoundRect(1+x, 1+y, 76+sx, 110, 4, TH.box_border);
  spr.fillSmoothRoundRect(2+x, 2+y, 74+sx, 108, 4, TH.box_bg);

  spr.drawString("Step:", 6+x, 64+y+(-3*16), 2);
  spr.drawString(getCurrentStep()->desc, 48+x, 64+y+(-3*16), 2);

  spr.drawString("BW:", 6+x, 64+y+(-2*16), 2);
  spr.drawString(getCurrentBandwidth()->desc, 48+x, 64+y+(-2*16), 2);

  if(!agcNdx && !agcIdx)
  {
    spr.drawString("AGC:", 6+x, 64+y+(-1*16), 2);
    spr.drawString("On", 48+x, 64+y+(-1*16), 2);
  }
  else
  {
    sprintf(text, "%2.2d", agcNdx);
    spr.drawString("Att:", 6+x, 64+y+(-1*16), 2);
    spr.drawString(text, 48+x, 64+y+(-1*16), 2);
  }

  spr.drawString("Vol:", 6+x, 64+y+(0*16), 2);
  if(muteOn() || squelchCutoff)
  {
    spr.setTextColor(TH.box_off_text, TH.box_off_bg);
    sprintf(text, muteOn() ? "Muted" : "%d/sq", volume);
    spr.drawString(text, 48+x, 64+y+(0*16), 2);
    spr.setTextColor(TH.box_text, TH.box_bg);
  }
  else
  {
    spr.setTextColor(TH.box_text, TH.box_bg);
    spr.drawNumber(volume, 48+x, 64+y+(0*16), 2);
  }

  // Draw RDS PI code, if present
  uint16_t piCode = getRdsPiCode();
  if(piCode && currentMode == FM)
  {
    sprintf(text, "%04X", piCode);
    spr.drawString("PI:", 6+x, 64+y + (1*16), 2);
    spr.drawString(text, 48+x, 64+y + (1*16), 2);
  }
  else
  {
    spr.drawString("AVC:", 6+x, 64+y + (1*16), 2);

    if(currentMode==FM)
      sprintf(text, "n/a");
    else if(isSSB())
      sprintf(text, "%2.2ddB", SsbAvcIdx);
    else
      sprintf(text, "%2.2ddB", AmAvcIdx);

    spr.drawString(text, 48+x, 64+y + (1*16), 2);
  }

  // Draw current time
  if(clockGet())
  {
    spr.drawString("Time:", 6+x, 64+y+(2*16), 2);
    spr.drawString(clockGet(), 48+x, 64+y+(2*16), 2);
  }
}

//
// Draw side bar (menu or information)
//
void drawSideBar(uint16_t cmd, int x, int y, int sx)
{
  if(sleepOn()) return;

  switch(cmd)
  {
    case CMD_MENU:      drawMenu(x, y, sx);      break;
    case CMD_SETTINGS:  drawSettings(x, y, sx);  break;
    case CMD_MODE:      drawMode(x, y, sx);      break;
    case CMD_STEP:      drawStep(x, y, sx);      break;
    case CMD_SEEK:      drawSeek(x, y, sx);      break;
    case CMD_BAND:      drawBand(x, y, sx);      break;
    case CMD_BANDWIDTH: drawBandwidth(x, y, sx); break;
    case CMD_THEME:     drawTheme(x, y, sx);     break;
    case CMD_UI:        drawUILayout(x, y, sx);  break;
    case CMD_VOLUME:    drawVolume(x, y, sx);    break;
    case CMD_AGC:       drawAgc(x, y, sx);       break;
    case CMD_SOFTMUTE:  drawSoftMuteMaxAtt(x, y, sx);break;
    case CMD_CAL:       drawCal(x, y, sx);       break;
    case CMD_AVC:       drawAvc(x, y, sx);       break;
    case CMD_FM_REGION: drawFmRegion(x, y, sx);  break;
    case CMD_BRT:       drawBrt(x, y, sx);       break;
    case CMD_RDS:       drawRDSMode(x, y, sx);   break;
    case CMD_MEMORY:    drawMemory(x, y, sx);    break;
    case CMD_SLEEP:     drawSleep(x, y, sx);     break;
    case CMD_SLEEPMODE: drawSleepMode(x, y, sx); break;
    case CMD_BLEMODE:   drawBleMode(x, y, sx);   break;
    case CMD_WIFIMODE:  drawWiFiMode(x, y, sx);  break;
    case CMD_ZOOM:      drawZoom(x, y, sx);      break;
    case CMD_SCROLL:    drawScrollDir(x, y, sx); break;
    case CMD_UTCOFFSET: drawUTCOffset(x, y, sx); break;
    case CMD_SQUELCH:   drawSquelch(x, y, sx);   break;
    default:            drawInfo(x, y, sx);      break;
  }
}
