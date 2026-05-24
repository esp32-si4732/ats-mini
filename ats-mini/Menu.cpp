#include "math.h"
#include "Common.h"
#include "Themes.h"
#include "Utils.h"
#include "Draw.h"
#include "EIBI.h"
#include "BleMode.h"
#include "Menu.h"
#include "MenuDraw.h"

//
// Bands Menu
//
// TO CONFIGURE YOUR OWN BAND PLAN:
// Add new bands by inserting new lines in the table below. Remove
// bands by deleting lines. Change bands by editing lines below.
//
// NOTE:
// You have to RESET PREFERENCES after adding or removing lines in this
// table. Turn your receiver on with the encoder push button pressed
// at first time to RESET the preferences.
//

int bandIdx = 0;

// Band limits are expanded to align with the nearest tuning scale mark
// Do not forget to update the bands table in the manual.md
Band bands[] =
{
  {"VHF",  FM_BAND_TYPE, FM,   6400, 10800, 10390, 2, 0, 0, 0},
  // All band. LW, MW and SW (from 150kHz to 30MHz)
  {"ALL",  SW_BAND_TYPE, AM,    150, 30000, 15000, 1, 4, 0, 0},
  {"11M",  SW_BAND_TYPE, AM,  25600, 26100, 25850, 1, 4, 0, 0},
  {"13M",  SW_BAND_TYPE, AM,  21500, 21900, 21650, 1, 4, 0, 0},
  {"15M",  SW_BAND_TYPE, AM,  18900, 19100, 18950, 1, 4, 0, 0},
  {"16M",  SW_BAND_TYPE, AM,  17400, 18100, 17650, 1, 4, 0, 0},
  {"19M",  SW_BAND_TYPE, AM,  15100, 15900, 15450, 1, 4, 0, 0},
  {"22M",  SW_BAND_TYPE, AM,  13500, 13900, 13650, 1, 4, 0, 0},
  {"25M",  SW_BAND_TYPE, AM,  11000, 13000, 11850, 1, 4, 0, 0},
  {"31M",  SW_BAND_TYPE, AM,   9000, 11000,  9650, 1, 4, 0, 0},
  {"41M",  SW_BAND_TYPE, AM,   7000,  9000,  7300, 1, 4, 0, 0},
  {"49M",  SW_BAND_TYPE, AM,   5000,  7000,  6000, 1, 4, 0, 0},
  {"60M",  SW_BAND_TYPE, AM,   4000,  5100,  4950, 1, 4, 0, 0},
  {"75M",  SW_BAND_TYPE, AM,   3500,  4000,  3950, 1, 4, 0, 0},
  {"90M",  SW_BAND_TYPE, AM,   3000,  3500,  3300, 1, 4, 0, 0},
//  {"25M",  SW_BAND_TYPE, AM,  11600, 12100, 11850, 1, 4, 0},
//  {"31M",  SW_BAND_TYPE, AM,   9400,  9900,  9650, 1, 4, 0},
//  {"41M",  SW_BAND_TYPE, AM,   7200,  7500,  7300, 1, 4, 0},
//  {"49M",  SW_BAND_TYPE, AM,   5900,  6200,  6000, 1, 4, 0},
//  {"60M",  SW_BAND_TYPE, AM,   4700,  5100,  4950, 1, 4, 0},
//  {"75M",  SW_BAND_TYPE, AM,   3900,  4000,  3950, 1, 4, 0},
//  {"90M",  SW_BAND_TYPE, AM,   3200,  3400,  3300, 1, 4, 0},
  {"MW3",  MW_BAND_TYPE, AM,   1700,  3500,  2500, 1, 4, 0, 0},
  {"MW2",  MW_BAND_TYPE, AM,    495,  1701,   783, 2, 4, 0, 0},
  {"MW1",  MW_BAND_TYPE, AM,    150,  1800,   810, 3, 4, 0, 0},
  {"160M", MW_BAND_TYPE, LSB,  1800,  2000,  1900, 5, 4, 0, 0},
  {"80M",  SW_BAND_TYPE, LSB,  3500,  4000,  3800, 5, 4, 0, 0},
  {"40M",  SW_BAND_TYPE, LSB,  7000,  7300,  7150, 5, 4, 0, 0},
  {"30M",  SW_BAND_TYPE, LSB, 10000, 10200, 10125, 5, 4, 0, 0},
  {"20M",  SW_BAND_TYPE, USB, 14000, 14400, 14100, 5, 4, 0, 0},
  {"17M",  SW_BAND_TYPE, USB, 18000, 18200, 18115, 5, 4, 0, 0},
  {"15M",  SW_BAND_TYPE, USB, 21000, 21500, 21225, 5, 4, 0, 0},
  {"12M",  SW_BAND_TYPE, USB, 24800, 25000, 24940, 5, 4, 0, 0},
  {"10M",  SW_BAND_TYPE, USB, 28000, 29700, 28500, 5, 4, 0, 0},
  // https://www.hfunderground.com/wiki/CB
  // Also see MIN_CB_FREQUENCY and MAX_CB_FREQUENCY
  {"CB",   SW_BAND_TYPE, AM,  25000, 28000, 27135, 0, 4, 0, 0},
};

int getTotalBands() { return(ITEM_COUNT(bands)); }
Band *getCurrentBand() { return(&bands[bandIdx]); }

//
// Main Menu
//

int8_t menuIdx = MENU_VOLUME;

const char *menu[] =
{
  "Mode",
  "Band",
  "Volume",
  "Step",
  "Seek",
  "Scan",
  "Memory",
  "Squelch",
  "Bandwidth",
  "AGC/ATTN",
  "AVC",
  "SoftMute",
  "Settings",
};

//
// Settings Menu
//

int8_t settingsIdx = MENU_BRIGHTNESS;

const char *settings[] =
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
  "USB Port",
  "Bluetooth",
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

const RDSMode rdsMode[] =
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

uint8_t getRDSMode() { return(rdsMode[radioState.rdsMode].mode); }
int getTotalRDSModes() { return(ITEM_COUNT(rdsMode)); }

//
// Sleep Mode Menu
//

const char *sleepModeDesc[] =
{ "Locked", "Unlocked", "CPU Sleep" };

//
// UTC Offset Menu
// https://en.wikipedia.org/wiki/List_of_UTC_offsets
// https://www.timeanddate.com/time/time-zones-interesting.html
//
const UTCOffset utcOffsets[] =
{
  { -12 * 4, "UTC-12" },
  { -11 * 4, "UTC-11" },
  { -10 * 4, "UTC-10" },
  { -9 * 4 - 2, "UTC-9:30" },
  { -9 * 4, "UTC-9" },
  { -8 * 4, "UTC-8" },
  { -7 * 4, "UTC-7" },
  { -6 * 4, "UTC-6" },
  { -5 * 4, "UTC-5" },
  { -4 * 4, "UTC-4" },
  { -3 * 4 - 2, "UTC-3:30" },
  { -3 * 4, "UTC-3" },
  { -2 * 4 - 2, "UTC-2:30" },
  { -2 * 4, "UTC-2" },
  { -1 * 4, "UTC-1" },
  {  0 * 4, "UTC+0" },
  {  1 * 4, "UTC+1" },
  {  2 * 4, "UTC+2" },
  {  3 * 4, "UTC+3" },
  {  3 * 4 + 2, "UTC+3:30" },
  {  4 * 4, "UTC+4" },
  {  4 * 4 + 2, "UTC+4:30" },
  {  5 * 4, "UTC+5" },
  {  5 * 4 + 2, "UTC+5:30" },
  {  5 * 4 + 3, "UTC+5:45" },
  {  6 * 4, "UTC+6" },
  {  6 * 4 + 2, "UTC+6:30" },
  {  7 * 4, "UTC+7" },
  {  8 * 4, "UTC+8" },
  {  8 * 4 + 3, "UTC+8:45" },
  {  9 * 4, "UTC+9" },
  {  9 * 4 + 2, "UTC+9:30" },
  { 10 * 4, "UTC+10" },
  { 10 * 4 + 2, "UTC+10:30" },
  { 11 * 4, "UTC+11" },
  { 12 * 4, "UTC+12" },
  { 12 * 4 + 3, "UTC+12:45" },
  { 13 * 4, "UTC+13" },
  { 13 * 4 + 3, "UTC+13:45" },
  { 14 * 4, "UTC+14" },
};

int getCurrentUTCOffset() { return(utcOffsets[radioState.utcOffset].offset); }
int getTotalUTCOffsets() { return(ITEM_COUNT(utcOffsets)); }
int getTotalSleepModes() { return(ITEM_COUNT(sleepModeDesc)); }

//
// UI Layout Menu
//
const char *uiLayoutDesc[] =
{ "Default", "S-Meter" };

int getTotalUILayouts() { return(ITEM_COUNT(uiLayoutDesc)); }

//
// USB Port Mode Menu
//

const char *usbModeDesc[] =
{ "Off", "Ad hoc" };

int getTotalUSBModes() { return(ITEM_COUNT(usbModeDesc)); }

//
// Bluetooth Mode Menu
//

const char *bleModeDesc[] =
{ "Off", "Ad hoc", "HID" };

int getTotalBleModes() { return(ITEM_COUNT(bleModeDesc)); }

//
// WiFi Mode Menu
//

const char *wifiModeDesc[] =
{ "Off", "AP Only", "AP+Connect", "Connect", "Sync Only" };

int getTotalWiFiModes() { return(ITEM_COUNT(wifiModeDesc)); }
int getTotalMenuItems() { return(ITEM_COUNT(menu)); }
int getTotalSettingsItems() { return(ITEM_COUNT(settings)); }

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

const Step *steps[4] = { fmSteps, ssbSteps, ssbSteps, amSteps };
static const uint8_t defaultStepIdx[4] = { 2, 5, 5, 1 };

int getLastStep(int mode)
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

const Step *getCurrentStep()
{
  uint8_t idx = bands[bandIdx].currentStepIdx > getLastStep(radioState.mode) ? defaultStepIdx[radioState.mode] : bands[bandIdx].currentStepIdx;
  return(&steps[radioState.mode][idx]);
}

static uint8_t freqInputPos = 0;

static uint8_t getDefaultFreqInputPos(int mode, int step)
{
  return (uint8_t)(log10(step) * 2) + (mode == AM ? 6 : 0);
}

void resetFreqInputPos()
{
  freqInputPos = getDefaultFreqInputPos(radioState.mode, getCurrentStep()->step);
}

uint8_t getFreqInputPos()
{
  return freqInputPos;
}

int getFreqInputStep()
{
  return freqInputPos % 2 ?
    5 * pow(10, (freqInputPos - (radioState.mode == AM ? 6 : 0) - 1) / 2) :
            pow(10, (freqInputPos - (radioState.mode == AM ? 6 : 0)) / 2);
}

static uint8_t getMinFreqInputPos()
{
  return getDefaultFreqInputPos(radioState.mode, steps[radioState.mode][0].step);
}

static uint8_t getMaxFreqInputPos()
{
  return (uint8_t)log10(getCurrentBand()->maximumFreq) * 2 + (radioState.mode != FM ? 6 : -2);
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

const Bandwidth *bandwidths[4] =
{
  fmBandwidths, ssbBandwidths, ssbBandwidths, amBandwidths
};

static const uint8_t defaultBwIdx[4] = { 0, 4, 4, 4 };

int getLastBandwidth(int mode)
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
  return(&bandwidths[radioState.mode][bands[bandIdx].bandwidthIdx > getLastBandwidth(radioState.mode) ? defaultBwIdx[radioState.mode] : bands[bandIdx].bandwidthIdx]);
}

static void setBandwidth()
{
  uint8_t idx = getCurrentBandwidth()->idx;

  switch(radioState.mode)
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
  if(radioState.mode == FM || !eibiAvailable() || !clockAvailable())
    return(SEEK_DEFAULT);

  return(mode);
}

//
// Utility functions to change menu values
//

static inline int min(int x, int y) { return(x<y? x:y); }

static inline int wrap_range(int v, int enc, int vMin, int vMax)
{
  v += enc;
  v  = v>vMax? vMin + (v - vMax - 1) % (vMax - vMin + 1) : v<vMin? vMax - (vMin - v - 1) % (vMax - vMin + 1) : v;
  return(v);
}

static inline int clamp_range(int v, int enc, int vMin, int vMax)
{
  v += enc;
  v  = v>vMax? vMax : v<vMin? vMin : v;
  return(v);
}

//
// Encoder input handlers
//

void doSelectDigit(int16_t enc)
{
  freqInputPos = clamp_range(freqInputPos, -enc, getMinFreqInputPos(), getMaxFreqInputPos());
}

void doVolume(int16_t enc)
{
  radioState.vol = clamp_range(radioState.vol, enc, 0, 63);
  if(!muteOn(MUTE_MAIN)) rx.setVolume(radioState.vol);
}

static void clickVolume(bool shortPress)
{
  if(shortPress) muteOn(MUTE_MAIN, !muteOn(MUTE_MAIN)); else radioState.cmd = CMD_NONE;
}

static void clickSquelch(bool shortPress)
{
  if(shortPress)
  {
    if(radioState.squelch[radioState.mode] & 0x7f)
      radioState.squelch[radioState.mode] &= 0x80;
    else
      radioState.squelch[radioState.mode] ^= 0x80;
  }
  else
  {
    radioState.cmd = CMD_NONE;
  }
}

static void clickSeek(bool shortPress)
{
  if(shortPress) seekMode(true); else radioState.cmd = CMD_NONE;
}

static void clickScan(bool shortPress)
{
  if(shortPress)
  {
    // Clear stale parameters
    clearStationInfo();
    rssi = snr = 0;
    drawScreen();
    drawMessage("Scanning...");
    scanRun(radioState.frequency, 10);
  }
  else radioState.cmd = CMD_NONE;
}

static void doTheme(int16_t enc)
{
  themeIdx = wrap_range(themeIdx, enc, 0, getTotalThemes() - 1);
}

static void doUILayout(int16_t enc)
{
  radioState.uiLayout = radioState.uiLayout > LAST_ITEM(uiLayoutDesc) ? UI_DEFAULT : wrap_range(radioState.uiLayout, enc, 0, LAST_ITEM(uiLayoutDesc));
}

void doAvc(int16_t enc)
{
  // Only allow for AM and SSB modes
  if(radioState.mode==FM) return;

  // wrap_range expects to wrap a range of incremental numbers. avc instead is a range of all even numbers
  int8_t newAvcIdx = wrap_range((isSSB() ? radioState.ssbAvcIdx : radioState.amAvcIdx) / 2, enc, 12 / 2, 90 / 2) * 2;
  if(isSSB())
  {
    radioState.ssbAvcIdx = newAvcIdx;
  }
  else
  {
    radioState.amAvcIdx = newAvcIdx;
  }
  rx.setAvcAmMaxGain(newAvcIdx);
}

void doFmRegion(int16_t enc)
{
  // Only allow for FM mode
  if(radioState.mode!=FM) return;

  radioState.fmRegionIdx = wrap_range(radioState.fmRegionIdx, enc, 0, LAST_ITEM(fmRegions));
  rx.setFMDeEmphasis(fmRegions[radioState.fmRegionIdx].value);
}

void doCal(int16_t enc)
{
  if (radioState.mode == USB)
    bands[bandIdx].usbCal = clamp_range(bands[bandIdx].usbCal, 10*enc, -MAX_CAL, MAX_CAL);
  else if (radioState.mode == LSB)
    bands[bandIdx].lsbCal = clamp_range(bands[bandIdx].lsbCal, 10*enc, -MAX_CAL, MAX_CAL);
  // else: no calibration change for other modes

  // If in SSB mode set the SI4732/5 BFO value
  // This adjusts the BFO while in the calibration menu
  if(isSSB()) updateBFO(radioState.bfo, true);
}

void doBrt(int16_t enc)
{
  radioState.brightness = clamp_range(radioState.brightness, 5*enc, 10, 255);
  if(!sleepOn()) ledcWrite(PIN_LCD_BL, radioState.brightness);
}

static void doSleep(int16_t enc)
{
  radioState.sleep = clamp_range(radioState.sleep, 5*enc, 0, 255);
}

static void doSleepMode(int16_t enc)
{
  radioState.sleepMode = wrap_range(radioState.sleepMode, enc, 0, LAST_ITEM(sleepModeDesc));
}

static void doUSBMode(int16_t enc)
{
  radioState.usbMode = wrap_range(radioState.usbMode, enc, 0, LAST_ITEM(usbModeDesc));
}

static void doBleMode(int16_t enc)
{
  radioState.bleMode = wrap_range(radioState.bleMode, enc, 0, LAST_ITEM(bleModeDesc));
}

static void doWiFiMode(int16_t enc)
{
  radioState.wifiMode = wrap_range(radioState.wifiMode, enc, 0, LAST_ITEM(wifiModeDesc));
}

static void clickBleMode(uint8_t mode, bool shortPress)
{
  radioState.cmd = CMD_NONE;
  bleInit(mode);
}

static void clickWiFiMode(uint8_t mode, bool shortPress)
{
  radioState.cmd = CMD_NONE;
  netInit(mode);
}

static void doRDSMode(int16_t enc)
{
  radioState.rdsMode = wrap_range(radioState.rdsMode, enc, 0, LAST_ITEM(rdsMode));
  if(!(getRDSMode() & RDS_CT)) clockReset();
}

static void doUTCOffset(int16_t enc)
{
  radioState.utcOffset = wrap_range(radioState.utcOffset, enc, 0, LAST_ITEM(utcOffsets));
  clockRefreshTime();
}

static void doZoom(int16_t enc)
{
  radioState.zoomLevel = !radioState.zoomLevel;
}

static void doScrollDir(int16_t enc)
{
  radioState.scrollDir = (radioState.scrollDir == 1) ? -1 : 1;
}

uint8_t doAbout(int16_t enc)
{
  static uint8_t aboutScreen = 0;
  aboutScreen = clamp_range(aboutScreen, enc, 0, 2);
  return aboutScreen;
}

bool tuneToMemory(const Memory *memory)
{
  uint16_t freq = freqFromHz(memory->freq, memory->mode);
  int bfo = bfoFromHz(memory->freq);

  // Must have frequency
  if(!memory->freq) return(false);

  // Must have valid band index
  if(memory->band>=getTotalBands()) return(false);

  // Band must contain frequency and modulation
  if(!isMemoryInBand(&bands[memory->band], memory)) return(false);

  // Must differ from the current band, frequency and modulation
  // FIXME: bands should store frequency in Hz (otherwise sub-kHz
  // digits are lost after a power cycle)
  if(memory->band==bandIdx && freq==bands[bandIdx].currentFreq && memory->mode==bands[bandIdx].bandMode)
    return(true);

  // Save current band settings
  bands[bandIdx].currentFreq = radioState.frequency + radioState.bfo / 1000;

  // Use default step when changing modes
  if(bands[memory->band].bandMode != memory->mode)
    bands[memory->band].currentStepIdx = defaultStepIdx[memory->mode];

  // Load frequency and modulation from memory slot
  bands[memory->band].currentFreq = freq;
  bands[memory->band].bandMode    = memory->mode;

  // Enable the new band
  selectBand(memory->band);

  // Update BFO if present in memory slot
  if(bfo) updateBFO(bfo);

  return(true);
}

static void doMemory(int16_t enc)
{
  memoryIdx = wrap_range(memoryIdx, enc, 0, LAST_ITEM(memories));
  if(!tuneToMemory(&memories[memoryIdx])) tuneToMemory(&newMemory);
}

static void clickMemory(uint8_t idx, bool shortPress)
{
  // Must have a valid index
  if(idx>LAST_ITEM(memories)) return;

  if(shortPress)
  {
    // If clicking on an empty memory slot, save to it
    if(!memories[idx].freq) memories[idx] = newMemory;
    // Otherwise, delete memory slot contents
    else memories[idx].freq = 0;
  }
  // On a click, do nothing, slot already activated in doMemory()
  else radioState.cmd = CMD_NONE;
}

void doStep(int16_t enc)
{
  uint8_t idx = bands[bandIdx].currentStepIdx;

  idx = wrap_range(idx, enc, 0, getLastStep(radioState.mode));
  bands[bandIdx].currentStepIdx = idx;

  rx.setFrequencyStep(steps[radioState.mode][idx].step);

  // Set seek spacing
  if(radioState.mode==FM)
    rx.setSeekFmSpacing(steps[radioState.mode][idx].spacing);
  else
    rx.setSeekAmSpacing(steps[radioState.mode][idx].spacing);
}

void doAgc(int16_t enc)
{
  if(radioState.mode==FM)
    radioState.agcIndex = radioState.fmAgcIdx = wrap_range(radioState.fmAgcIdx, enc, 0, 27);
  else if(isSSB())
    radioState.agcIndex = radioState.ssbAgcIdx = wrap_range(radioState.ssbAgcIdx, enc, 0, 1);
  else
    radioState.agcIndex = radioState.amAgcIdx = wrap_range(radioState.amAgcIdx, enc, 0, 37);

  // Process radioState.agcIndex to generate radioState.agcDisable and radioState.agcIndex
  // radioState.agcIndex     0 1 2 3 4 5 6  ..... n    (n:    FM = 27, AM = 37, SSB = 1)
  // radioState.agcNdxVal     0 0 1 2 3 4 5  ..... n -1 (n -1: FM = 26, AM = 36, SSB = 0)
  // radioState.agcDisable 0 1 1 1 1 1 1  ..... 1

  // if true, disable AGC; else, AGC is enabled
  radioState.agcDisable = radioState.agcIndex>0? 1 : 0;
  radioState.agcNdxVal     = radioState.agcIndex>1? radioState.agcIndex - 1 : 0;

  // Configure SI4732/5 (if radioState.agcNdxVal = 0, no attenuation)
  rx.setAutomaticGainControl(radioState.agcDisable, radioState.agcNdxVal);
}

void doMode(int16_t enc)
{
  // This is our current mode for the current band
  radioState.mode = bands[bandIdx].bandMode;

  // Cannot change away from FM mode
  if(radioState.mode==FM) return;

  // Change AM/LSB/USB modes, do not allow FM mode
  do
    radioState.mode = wrap_range(radioState.mode, enc, 0, LAST_ITEM(bandModeDesc));
  while(radioState.mode==FM);

  // Save current band settings
  bands[bandIdx].currentFreq = radioState.frequency + radioState.bfo / 1000;
  bands[bandIdx].currentStepIdx = defaultStepIdx[radioState.mode];
  bands[bandIdx].bandwidthIdx = defaultBwIdx[radioState.mode];
  bands[bandIdx].bandMode = radioState.mode;

  // Enable the new band
  selectBand(bandIdx);
}

void doSquelch(int16_t enc)
{
  uint8_t squelchParam = radioState.squelch[radioState.mode] & 0x80;
  uint8_t squelchValue = radioState.squelch[radioState.mode] & 0x7f;
  radioState.squelch[radioState.mode] = squelchParam | clamp_range(squelchValue, enc, 0, 0x7f);
}

void doSoftMute(int16_t enc)
{
  // Nothing to do if FM mode
  if(radioState.mode==FM) return;

  if(isSSB())
    radioState.softMuteMaxAtt = radioState.ssbSoftMuteIdx = wrap_range(radioState.ssbSoftMuteIdx, enc, 0, 32);
  else
    radioState.softMuteMaxAtt = radioState.amSoftMuteIdx = wrap_range(radioState.amSoftMuteIdx, enc, 0, 32);

  rx.setAmSoftMuteMaxAttenuation(radioState.softMuteMaxAtt);
}

void doBand(int16_t enc)
{
  // Save current band settings
  bands[bandIdx].currentFreq = radioState.frequency + radioState.bfo / 1000;
  bands[bandIdx].bandMode = radioState.mode;

  // Change band
  bandIdx = wrap_range(bandIdx, enc, 0, LAST_ITEM(bands));

  // Enable the new band
  selectBand(bandIdx);
}

void doBandwidth(int16_t enc)
{
  uint8_t idx = bands[bandIdx].bandwidthIdx;

  idx = wrap_range(idx, enc, 0, getLastBandwidth(radioState.mode));
  bands[bandIdx].bandwidthIdx = idx;
  setBandwidth();
}

//
// Handle encoder input in menu
//

static void doMenu(int16_t enc)
{
  menuIdx = wrap_range(menuIdx, enc, 0, LAST_ITEM(menu));
}

static void clickMenu(int cmd, bool shortPress)
{
  // No command yet
  radioState.cmd = CMD_NONE;

  switch(cmd)
  {
    case MENU_STEP:     radioState.cmd = CMD_STEP;      break;
    case MENU_SEEK:     radioState.cmd = CMD_SEEK;      break;
    case MENU_MODE:     radioState.cmd = CMD_MODE;      break;
    case MENU_BW:       radioState.cmd = CMD_BANDWIDTH; break;
    case MENU_AGC_ATT:  radioState.cmd = CMD_AGC;       break;
    case MENU_BAND:     radioState.cmd = CMD_BAND;      break;
    case MENU_SETTINGS: radioState.cmd = CMD_SETTINGS;  break;
    case MENU_SQUELCH:  radioState.cmd = CMD_SQUELCH;   break;
    case MENU_VOLUME:   radioState.cmd = CMD_VOLUME;    break;

    case MENU_MEMORY:
      radioState.cmd = CMD_MEMORY;
      newMemory.freq  = freqToHz(radioState.frequency, radioState.mode) + radioState.bfo;
      newMemory.mode  = radioState.mode;
      newMemory.band  = bandIdx;
      doMemory(0);
      break;

    case MENU_SOFTMUTE:
      // No soft mute in FM mode
      if(radioState.mode!=FM) radioState.cmd = CMD_SOFTMUTE;
      break;

    case MENU_AVC:
      // No AVC in FM mode
      if(radioState.mode!=FM) radioState.cmd = CMD_AVC;
      break;

    case MENU_SCAN:
      // Run a band scan around current frequency with the same
      // step as scale resolution (10kHz for AM, 100kHz for FM)
      radioState.cmd = CMD_SCAN;
      clickScan(true);
      break;
  }
}

static void doSettings(int16_t enc)
{
  settingsIdx = wrap_range(settingsIdx, enc, 0, LAST_ITEM(settings));
}

static void clickSettings(int cmd, bool shortPress)
{
  // No command yet
  radioState.cmd = CMD_NONE;

  switch(cmd)
  {
    case MENU_BRIGHTNESS: radioState.cmd = CMD_BRT; break;
    case MENU_CALIBRATION:
      if(isSSB()) radioState.cmd = CMD_CAL;
      break;
    case MENU_THEME:      radioState.cmd = CMD_THEME;      break;
    case MENU_UI:         radioState.cmd = CMD_UI;         break;
    case MENU_RDS:        radioState.cmd = CMD_RDS;        break;
    case MENU_ZOOM:       radioState.cmd = CMD_ZOOM;       break;
    case MENU_SCROLL:     radioState.cmd = CMD_SCROLL;     break;
    case MENU_SLEEP:      radioState.cmd = CMD_SLEEP;      break;
    case MENU_SLEEPMODE:  radioState.cmd = CMD_SLEEPMODE;  break;
    case MENU_UTCOFFSET:  radioState.cmd = CMD_UTCOFFSET;  break;
    case MENU_USBMODE:    radioState.cmd = CMD_USBMODE;    break;
    case MENU_BLEMODE:    radioState.cmd = CMD_BLEMODE;    break;
    case MENU_WIFIMODE:   radioState.cmd = CMD_WIFIMODE;   break;
    case MENU_FM_REGION:
      // Only in FM mode
      if(radioState.mode==FM) radioState.cmd = CMD_FM_REGION;
      break;
    case MENU_ABOUT:      radioState.cmd = CMD_ABOUT;     break;

    case MENU_LOADEIBI:
      eibiLoadSchedule();
      break;
  }
}

bool doSideBar(uint16_t cmd, int16_t enc, int16_t enca)
{
  // Ignore idle encoder
  if(!enc) return(false);

  switch(cmd)
  {
    // Menus and list-based options must take radioState.scrollDir into account
    case CMD_MENU:       doMenu(radioState.scrollDir * enc);break;
    case CMD_MODE:       doMode(radioState.scrollDir * enc);break;
    case CMD_STEP:       doStep(radioState.scrollDir * enc);break;
    case CMD_AGC:        doAgc(enc);break;
    case CMD_BANDWIDTH:  doBandwidth(radioState.scrollDir * enc);break;
    case CMD_VOLUME:     doVolume(enca);break;
    case CMD_SOFTMUTE:   doSoftMute(enc);break;
    case CMD_BAND:       doBand(radioState.scrollDir * enc);break;
    case CMD_AVC:        doAvc(enc);break;
    case CMD_FM_REGION:  doFmRegion(radioState.scrollDir * enc);break;
    case CMD_SETTINGS:   doSettings(radioState.scrollDir * enc);break;
    case CMD_BRT:        doBrt(enca);break;
    case CMD_CAL:        doCal(enca);break;
    case CMD_THEME:      doTheme(radioState.scrollDir * enc);break;
    case CMD_UI:         doUILayout(radioState.scrollDir * enc);break;
    case CMD_RDS:        doRDSMode(radioState.scrollDir * enc);break;
    case CMD_MEMORY:     doMemory(radioState.scrollDir * enca);break;
    case CMD_SLEEP:      doSleep(enca);break;
    case CMD_SLEEPMODE:  doSleepMode(radioState.scrollDir * enc);break;
    case CMD_USBMODE:    doUSBMode(radioState.scrollDir * enc);break;
    case CMD_BLEMODE:    doBleMode(radioState.scrollDir * enc);break;
    case CMD_WIFIMODE:   doWiFiMode(radioState.scrollDir * enc);break;
    case CMD_ZOOM:       doZoom(enc);break;
    case CMD_SCROLL:     doScrollDir(enc);break;
    case CMD_UTCOFFSET:  doUTCOffset(radioState.scrollDir * enc);break;
    case CMD_SQUELCH:    doSquelch(enca);break;
    case CMD_ABOUT:      doAbout(enc);break;
    default:             return(false);
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
    case CMD_BLEMODE:  clickBleMode(radioState.bleMode, shortPress);break;
    case CMD_WIFIMODE: clickWiFiMode(radioState.wifiMode, shortPress);break;
    case CMD_VOLUME:   clickVolume(shortPress);break;
    case CMD_SQUELCH:  clickSquelch(shortPress);break;
    case CMD_SEEK:     clickSeek(shortPress);break;
    case CMD_SCAN:     clickScan(shortPress);break;
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
  muteOn(MUTE_TEMP, true);

  // Set band and mode
  bandIdx = min(idx, LAST_ITEM(bands));
  radioState.mode = bands[bandIdx].bandMode;

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
  identifyFrequency(radioState.frequency + radioState.bfo / 1000);

  // Set default digit position based on the current step
  resetFreqInputPos();

  // Unmute the sound
  muteOn(MUTE_TEMP, false);
}
