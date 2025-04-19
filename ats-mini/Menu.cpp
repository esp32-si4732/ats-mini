#include "Common.h"
#include "Themes.h"
#include "Menu.h"

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
Band band[] =
{
  {"VHF",  FM_BAND_TYPE, FM,   6400, 10800, 10390, 1, 0, 0},
  // All band. LW, MW and SW (from 150kHz to 30MHz)
  {"ALL",  SW_BAND_TYPE, AM,    150, 30000, 15000, 0, 4, 0},
  {"11M",  SW_BAND_TYPE, AM,  25600, 26100, 25850, 1, 4, 0},
  {"13M",  SW_BAND_TYPE, AM,  21500, 21900, 21650, 1, 4, 0},
  {"15M",  SW_BAND_TYPE, AM,  18900, 19100, 18950, 1, 4, 0},
  {"16M",  SW_BAND_TYPE, AM,  17500, 18100, 17650, 1, 4, 0},
  {"19M",  SW_BAND_TYPE, AM,  15100, 15900, 15450, 1, 4, 0},
  {"22M",  SW_BAND_TYPE, AM,  13500, 13900, 13650, 1, 4, 0},
  {"25M",  SW_BAND_TYPE, AM,  11600, 12100, 11850, 1, 4, 0},
  {"31M",  SW_BAND_TYPE, AM,   9400,  9900,  9650, 1, 4, 0},
  {"41M",  SW_BAND_TYPE, AM,   7200,  7500,  7300, 1, 4, 0},
  {"49M",  SW_BAND_TYPE, AM,   5900,  6200,  6000, 1, 4, 0},
  {"60M",  SW_BAND_TYPE, AM,   4700,  5100,  4950, 1, 4, 0},
  {"75M",  SW_BAND_TYPE, AM,   3900,  4000,  3950, 1, 4, 0},
  {"90M",  SW_BAND_TYPE, AM,   3200,  3400,  3300, 1, 4, 0},
  {"MW3",  MW_BAND_TYPE, AM,   1700,  3500,  2500, 1, 4, 0},
  {"MW2",  MW_BAND_TYPE, AM,    495,  1701,   783, 2, 4, 0},
  {"MW1",  MW_BAND_TYPE, AM,    150,  1800,   810, 3, 4, 0},
  {"160M", MW_BAND_TYPE, LSB,  1800,  2000,  1900, 0, 4, 0},
  {"80M",  MW_BAND_TYPE, LSB,  3500,  4000,  3800, 0, 4, 0},
  {"40M",  SW_BAND_TYPE, LSB,  7000,  7300,  7150, 0, 4, 0},
  {"30M",  SW_BAND_TYPE, LSB, 10000, 10200, 10125, 0, 4, 0},
  {"20M",  SW_BAND_TYPE, USB, 14000, 14400, 14100, 0, 4, 0},
  {"17M",  SW_BAND_TYPE, USB, 18000, 18200, 18115, 0, 4, 0},
  {"15M",  SW_BAND_TYPE, USB, 21000, 21500, 21225, 0, 4, 0},
  {"12M",  SW_BAND_TYPE, USB, 24800, 25000, 24940, 0, 4, 0},
  {"10M",  SW_BAND_TYPE, USB, 28000, 29700, 28500, 0, 4, 0},
  // https://www.hfunderground.com/wiki/CB
  {"CB",   SW_BAND_TYPE, AM,  25000, 30000, 27135, 0, 4, 0},
};

int getTotalBands() { return(ITEM_COUNT(band)); }
Band *getCurrentBand() { return(&band[bandIdx]); }

//
// Main Menu
//

#define MENU_MODE         0
#define MENU_BAND         1
#define MENU_VOLUME       2
#define MENU_STEP         3
#define MENU_BW           4
#define MENU_MUTE         5
#define MENU_AGC_ATT      6
#define MENU_SOFTMUTE     7
#define MENU_AVC          8
#define MENU_SETTINGS     9

int8_t menuIdx = MENU_VOLUME;

const char *menu[] =
{
  "Mode",
  "Band",
  "Volume",
  "Step",
  "Bandwidth",
  "Mute",
  "AGC/ATTN",
  "SoftMute",
  "AVC",
  "Settings",
};

//
// Settings Menu
//

#define MENU_BRIGHTNESS   0
#define MENU_CALIBRATION  1
#define MENU_SLEEP        2
#define MENU_THEME        3
#define MENU_ABOUT        4

int8_t settingsIdx = MENU_BRIGHTNESS;

const char *settings[] =
{
  "Brightness",
  "Calibration",
  "Sleep",
  "Theme",
  "About",
};

//
// Mode Menu
//

const char *bandModeDesc[] = { "FM", "LSB", "USB", "AM" };

//
// Step Menu
//

uint8_t AmTotalSteps    = 7; // Total AM steps
uint8_t AmTotalStepsSsb = 4; // G8PTN: Original : AM(LW/MW) 1k, 5k, 9k, 10k, 50k        : SSB 1k, 5k, 9k, 10k
uint8_t SsbTotalSteps   = 5; // SSB sub 1kHz steps

int fmStepIdx = 1;
Step fmStep[] =
{
  {   5, "50k"  },
  {  10, "100k" },
  {  20, "200k" },
  { 100, "1M"   },
};

volatile int8_t amStepIdx = 3;
Step amStep[] =
{
  {    1, "1k"   },  // 0   AM/SSB   (kHz)
  {    5, "5k"   },  // 1   AM/SSB   (kHz)
  {    9, "9k"   },  // 2   AM/SSB   (kHz)
  {   10, "10k"  },  // 3   AM/SSB   (kHz)
  {   50, "50k"  },  // 4   AM       (kHz)
  {  100, "100k" },  // 5   AM       (kHz)
  { 1000, "1M"   },  // 6   AM       (kHz)
  {   10, "10Hz" },  // 7   SSB      (Hz)
  {   25, "25Hz" },  // 8   SSB      (Hz)
  {   50, "50Hz" },  // 9   SSB      (Hz)
  {  100, "0.1k" },  // 10  SSB      (Hz)
  {  500, "0.5k" },  // 11  SSB      (Hz)
};

int ssbFastStep[] =
{
  1,   // 0->1 (1kHz -> 5kHz)
  3,   // 1->3 (5kHz -> 10kHz)
  2,   // 2->2 (9kHz -> 9kHz)
  3,   // 3->3 (10kHz -> 10kHz)
  4,   // 4->4 (50kHz -> 50kHz) n/a
  5,   // 5->5 (100kHz -> 100kHz) n/a
  6,   // 6->6 (1MHz -> 1MHz) n/a
  10,  // 7->10 (10Hz -> 100Hz)
  10,  // 8->10 (25Hz -> 100Hz)
  11,  // 9->11 (50Hz -> 500Hz)
  0,   // 10->0 (100Hz -> 1kHz)
  1,   // 11->1 (500Hz -> 5kHz)
};

const Step *getCurrentStep()
{
  return(currentMode==FM?  &fmStep[fmStepIdx] : &amStep[amStepIdx]);
}

int getLastStep()
{
  if(isSSB())
    return(AmTotalSteps + SsbTotalSteps - 1);
  else if(currentMode==FM)
    return(LAST_ITEM(fmStep));
  // G8PTN; Added in place of check in doStep() for LW/MW step limit
  else if(band[bandIdx].bandType == LW_BAND_TYPE || band[bandIdx].bandType == MW_BAND_TYPE)
    return(AmTotalStepsSsb);
  else
    return(AmTotalSteps - 1);
}

// @@@ FIXME: This function is only used for SSB tuning!
int getSteps(bool fast)
{
  if(isSSB())
  {
    // SSB: Return in Hz used for VFO + BFO tuning
    int i = fast? ssbFastStep[amStepIdx] : amStepIdx;
    return(amStep[i].step * (i>=AmTotalSteps? 1 : 1000));
  }
  else if(currentMode==FM)
  {
    return(fmStep[fmStepIdx].step);
  }
  else
  {
    // AM: Set to 0kHz if step is from the SSB Hz values
    // @@@ FIXME!!!
    if(amStepIdx>=AmTotalSteps) amStepIdx = 0;
    // AM: Return value in KHz for SI4732 step
    return(amStep[amStepIdx].step);
  }
}

//
// Bandwidth Menu
//

int8_t bwIdxSSB = 4;
Bandwidth bandwidthSSB[] =
{
  {4, "0.5k"},
  {5, "1.0k"},
  {0, "1.2k"},
  {1, "2.2k"},
  {2, "3.0k"},
  {3, "4.0k"}
};

int8_t bwIdxAM = 4;
Bandwidth bandwidthAM[] =
{
  {4, "1.0k"},
  {5, "1.8k"},
  {3, "2.0k"},
  {6, "2.5k"},
  {2, "3.0k"},
  {1, "4.0k"},
  {0, "6.0k"}
};

int8_t bwIdxFM = 0;
Bandwidth bandwidthFM[] =
{
  {0, "Auto"}, // Automatic - default
  {1, "110k"}, // Force wide (110 kHz) channel filter.
  {2, "84k"},
  {3, "60k"},
  {4, "40k"}
};

const Bandwidth *getCurrentBandwidth()
{
  if (isSSB())
    return &bandwidthSSB[bwIdxSSB];
  else if (currentMode == FM)
    return &bandwidthFM[bwIdxFM];
  else
    return &bandwidthAM[bwIdxAM];
}


void setSsbBandwidth(int idx)
{
  // Set Audio
  rx.setSSBAudioBandwidth(bandwidthSSB[idx].idx);

  // If audio bandwidth selected is about 2 kHz or below, it is
  // recommended to set Sideband Cutoff Filter to 0.
  if(bandwidthSSB[idx].idx==0 || bandwidthSSB[idx].idx==4 || bandwidthSSB[idx].idx==5)
    rx.setSSBSidebandCutoffFilter(0);
  else
    rx.setSSBSidebandCutoffFilter(1);
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

static void doMenu(int dir)
{
  menuIdx = wrap_range(menuIdx, dir, 0, LAST_ITEM(menu));
}

static void clickMenu(int cmd)
{
  // No command yet
  currentCmd = CMD_NONE;

  switch(cmd)
  {
    case MENU_STEP:     currentCmd = CMD_STEP; break;
    case MENU_MODE:     currentCmd = CMD_MODE; break;
    case MENU_BW:       currentCmd = CMD_BANDWIDTH; break;
    case MENU_AGC_ATT:  currentCmd = CMD_AGC; break;
    case MENU_BAND:     currentCmd = CMD_BAND; break;
    case MENU_SETTINGS: currentCmd = CMD_SETTINGS; break;

    case MENU_SOFTMUTE:
      // No soft mute in FM mode
      if(currentMode!=FM) currentCmd = CMD_SOFTMUTEMAXATT;
      break;

    case MENU_AVC:
      // No AVC in FM mode
      if(currentMode!=FM) currentCmd = CMD_AVC;
      break;

    case MENU_VOLUME:
      // Unmute sound when changing volume
      muteOn(false);
      currentCmd = CMD_VOLUME;
      break;

    case MENU_MUTE:
      muteOn(!muteOn());
      break;
  }
}

static void doSettings(int dir)
{
  settingsIdx = wrap_range(settingsIdx, dir, 0, LAST_ITEM(settings));
}

static void clickSettings(int cmd)
{
  // No command yet
  currentCmd = CMD_NONE;

  switch(cmd)
  {
    case MENU_BRIGHTNESS: currentCmd = CMD_BRT; break;
    case MENU_CALIBRATION:
      if(isSSB()) currentCmd = CMD_CAL;
      break;
    case MENU_SLEEP:      currentCmd = CMD_SLEEP; break;
    case MENU_THEME:      currentCmd = CMD_THEME; break;
    case MENU_ABOUT:      currentCmd = CMD_ABOUT; break;
  }
}

void doVolume(int dir)
{
  if(dir>0) rx.volumeUp(); else if(dir<0) rx.volumeDown();
  volume = rx.getVolume();
}

static void doTheme(int dir)
{
  themeIdx = wrap_range(themeIdx, dir, 0, getTotalThemes() - 1);
}

void doAvc(int dir)
{
  // Only allow for AM and SSB modes
  if(currentCmd==FM) return;

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

static void doCal(int dir)
{
  band[bandIdx].bandCal = clamp_range(band[bandIdx].bandCal, 10*dir, -MAX_CAL, MAX_CAL);

  // If in SSB mode set the SI4732/5 BFO value
  // This adjusts the BFO while in the calibration menu
  if(isSSB()) updateBFO(currentBFO);
}

void doBrt(int dir)
{
  currentBrt = clamp_range(currentBrt, 32*dir, 32, 255);

  if(displayOn()) ledcWrite(PIN_LCD_BL, currentBrt);
}

static void doSleep(int dir)
{
  currentSleep = clamp_range(currentSleep, 5*dir, 0, 255);
}

void doStep(int dir)
{
  int lastStep = getLastStep();

  if(currentMode==FM)
  {
    fmStepIdx = wrap_range(fmStepIdx, dir, 0, lastStep);
    band[bandIdx].currentStepIdx = fmStepIdx;
    rx.setFrequencyStep(fmStep[fmStepIdx].step);
  }
  else
  {
    amStepIdx = wrap_range(amStepIdx, dir, 0, lastStep);

    // SSB step limit
    if(isSSB() && amStepIdx>=AmTotalStepsSsb && amStepIdx<AmTotalSteps)
      amStepIdx = dir>0? AmTotalSteps : AmTotalStepsSsb-1;

    band[bandIdx].currentStepIdx = amStepIdx;

    // ?????
    if(!isSSB() || (amStepIdx<AmTotalSteps))
      rx.setFrequencyStep(amStep[amStepIdx].step);

    // Max 10kHz for spacing
    rx.setSeekAmSpacing(5);
  }
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
  currentMode = band[bandIdx].bandMode;

  // Cannot change away from FM mode
  if(currentMode==FM) return;

  // Change AM/LSB/USB modes, do not allow FM mode
  do
    currentMode = wrap_range(currentMode, dir, 0, LAST_ITEM(bandModeDesc));
  while(currentMode==FM);

  // Save current band settings
  band[bandIdx].currentFreq = currentFrequency + currentBFO / 1000;
  band[bandIdx].currentStepIdx = amStepIdx;
  band[bandIdx].bandMode = currentMode;

  // Enable the new band
  selectBand(bandIdx);
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
  band[bandIdx].currentFreq = currentFrequency + currentBFO / 1000;
  band[bandIdx].currentStepIdx = currentMode==FM? fmStepIdx:amStepIdx;

  // Change band
  bandIdx = wrap_range(bandIdx, dir, 0, LAST_ITEM(band));

  // Enable the new band
  selectBand(bandIdx);
}

void doBandwidth(int dir)
{
  if(isSSB())
  {
    bwIdxSSB = wrap_range(bwIdxSSB, dir, 0, LAST_ITEM(bandwidthSSB));
    setSsbBandwidth(bwIdxSSB);
    band[bandIdx].bandwidthIdx = bwIdxSSB;
  }
  else if(currentMode==AM)
  {
    bwIdxAM = wrap_range(bwIdxAM, dir, 0, LAST_ITEM(bandwidthAM));
    rx.setBandwidth(bandwidthAM[bwIdxAM].idx, 1);
    band[bandIdx].bandwidthIdx = bwIdxAM;
  }
  else
  {
    bwIdxFM = wrap_range(bwIdxFM, dir, 0, LAST_ITEM(bandwidthFM));
    rx.setFmBandwidth(bandwidthFM[bwIdxFM].idx);
    band[bandIdx].bandwidthIdx = bwIdxFM;
  }
}

//
// Handle encoder input in menu
//

bool doSideBar(uint16_t cmd, int dir)
{
  // Ignore idle encoder
  if(!dir) return(false);

  switch(cmd)
  {
    case CMD_MENU:      doMenu(dir);break;
    case CMD_MODE:      doMode(dir);break;
    case CMD_STEP:      doStep(dir);break;
    case CMD_AGC:       doAgc(dir);break;
    case CMD_BANDWIDTH: doBandwidth(dir);break;
    case CMD_VOLUME:    doVolume(dir);break;
    case CMD_SOFTMUTEMAXATT: doSoftMute(dir);break;
    case CMD_BAND:      doBand(dir);break;
    case CMD_AVC:       doAvc(dir);break;
    case CMD_SETTINGS:  doSettings(dir);break;
    case CMD_BRT:       doBrt(dir);break;
    case CMD_CAL:       doCal(dir);break;
    case CMD_SLEEP:     doSleep(dir);break;
    case CMD_THEME:     doTheme(dir);break;
    case CMD_ABOUT:     return(true);
    default:            return(false);
  }

  // Encoder input handled
  return(true);
}

bool clickSideBar(uint16_t cmd)
{
  switch(cmd)
  {
    case CMD_MENU:     clickMenu(menuIdx);break;
    case CMD_SETTINGS: clickSettings(settingsIdx);break;
    default:           return(false);
  }

  // Encoder input handled
  return(true);
}

void clickVolume()
{
  clickMenu(MENU_VOLUME);
}

//
// Selecting given band
//

void selectBand(uint8_t idx, bool drawLoadingSSB)
{
  // Set band and mode
  bandIdx = min(idx, LAST_ITEM(band));
  currentMode = band[bandIdx].bandMode;

  // Set tuning step
  if(band[bandIdx].bandType==FM_BAND_TYPE)
  {
    fmStepIdx = band[bandIdx].currentStepIdx;
    rx.setFrequencyStep(fmStep[fmStepIdx].step);
  }
  else
  {
    amStepIdx = band[bandIdx].currentStepIdx;
    rx.setFrequencyStep(amStep[amStepIdx].step);
  }

  int bwIdx = band[bandIdx].bandwidthIdx;

  // Load SSB patch as needed and set bandwidth
  if(isSSB())
  {
    loadSSB(bandwidthSSB[bwIdxSSB].idx, drawLoadingSSB);
    bwIdxSSB = min(bwIdx, LAST_ITEM(bandwidthSSB));
    setSsbBandwidth(bwIdxSSB);
  }
  else if(currentMode==FM)
  {
    unloadSSB();
    bwIdxFM = min(bwIdx, LAST_ITEM(bandwidthFM));
    rx.setFmBandwidth(bandwidthFM[bwIdxFM].idx);
  }
  else
  {
    unloadSSB();
    bwIdxAM = min(bwIdx, LAST_ITEM(bandwidthAM));
    rx.setBandwidth(bandwidthAM[bwIdxAM].idx, 1);
  }

  // Switch radio to the selected band
  useBand(&band[bandIdx]);
}

//
// Draw functions
//

static void drawCommon(const char *title, int x, int y, int sx)
{
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_hdr, TH.menu_bg);
  spr.fillSmoothRoundRect(1+x, 1+y, 76+sx, 110, 4, TH.menu_border);
  spr.fillSmoothRoundRect(2+x, 2+y, 74+sx, 108, 4, TH.menu_bg);

  spr.drawString(title, 40+x+(sx/2), 12+y, 2);
  spr.drawLine(1+x, 23+y, 76+sx, 23+y, TH.menu_border);

  spr.setTextFont(0);
  spr.setTextColor(TH.menu_item, TH.menu_bg);
  spr.fillRoundRect(6+x, 24+y+(2*16), 66+sx, 16, 2, TH.menu_hl_bg);
}

static void drawMenu(int x, int y, int sx)
{
  spr.setTextDatum(MC_DATUM);

  spr.fillSmoothRoundRect(1+x, 1+y, 76+sx, 110, 4, TH.menu_border);
  spr.fillSmoothRoundRect(2+x, 2+y, 74+sx, 108, 4, TH.menu_bg);
  spr.setTextColor(TH.menu_hdr, TH.menu_bg);

  char label_menu[16];
  sprintf(label_menu, "Menu %2.2d/%2.2d", menuIdx + 1, ITEM_COUNT(menu));
  spr.drawString(label_menu, 40+x+(sx/2), 12+y, 2);
  spr.drawLine(1+x, 23+y, 76+sx, 23+y, TH.menu_border);

  spr.setTextFont(0);
  spr.setTextColor(TH.menu_item, TH.menu_bg);
  spr.fillRoundRect(6+x, 24+y+(2*16), 66+sx, 16, 2, TH.menu_hl_bg);

  int count = ITEM_COUNT(menu);
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0)
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    else
      spr.setTextColor(TH.menu_item, TH.menu_bg);

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
    if(i==0)
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    else
      spr.setTextColor(TH.menu_item, TH.menu_bg);

    spr.drawString(settings[abs((settingsIdx+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawMode(int x, int y, int sx)
{
  drawCommon(menu[MENU_MODE], x, y, sx);

  int count = ITEM_COUNT(bandModeDesc);
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0)
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    else
      spr.setTextColor(TH.menu_item, TH.menu_bg);

    if((currentMode!=FM) || (i==0))
      spr.drawString(bandModeDesc[abs((currentMode+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawStep(int x, int y, int sx)
{
  int stepCount = getLastStep() + 1;

  drawCommon(menu[MENU_STEP], x, y, sx);

  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0)
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    else
      spr.setTextColor(TH.menu_item, TH.menu_bg);

    if(currentMode==FM)
      spr.drawString(fmStep[abs((fmStepIdx+stepCount+i)%stepCount)].desc, 40+x+(sx/2), 64+y+(i*16), 2);
    else
      spr.drawString(amStep[abs((amStepIdx+stepCount+i)%stepCount)].desc, 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawBand(int x, int y, int sx)
{
  drawCommon(menu[MENU_BAND], x, y, sx);

  int count = ITEM_COUNT(band);
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0)
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    else
      spr.setTextColor(TH.menu_item, TH.menu_bg);

    spr.drawString(band[abs((bandIdx+count+i)%count)].bandName, 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawBandwidth(int x, int y, int sx)
{
  drawCommon(menu[MENU_BW], x, y, sx);

  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0)
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    else
      spr.setTextColor(TH.menu_item, TH.menu_bg);

    if(isSSB())
    {
      int count = ITEM_COUNT(bandwidthSSB);
      spr.drawString(bandwidthSSB[abs((bwIdxSSB+count+i)%count)].desc, 40+x+(sx/2), 64+y+(i*16), 2);
    }
    else if(currentMode==FM)
    {
      int count = ITEM_COUNT(bandwidthFM);
      spr.drawString(bandwidthFM[abs((bwIdxFM+count+i)%count)].desc, 40+x+(sx/2), 64+y+(i*16), 2);
    }
    else
    {
      int count = ITEM_COUNT(bandwidthAM);
      spr.drawString(bandwidthAM[abs((bwIdxAM+count+i)%count)].desc, 40+x+(sx/2), 64+y+(i*16), 2);
     }
  }
}

static void drawTheme(int x, int y, int sx)
{
  drawCommon(settings[MENU_THEME], x, y, sx);

  int count = getTotalThemes();
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0)
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    else
      spr.setTextColor(TH.menu_item, TH.menu_bg);

    spr.drawString(theme[abs((themeIdx+count+i)%count)].name, 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawVolume(int x, int y, int sx)
{
  drawCommon(menu[MENU_VOLUME], x, y, sx);

  spr.setTextColor(TH.menu_param, TH.menu_bg);
  spr.fillRoundRect(6+x, 24+y+(2*16), 66+sx, 16, 2, TH.menu_bg);
  spr.drawNumber(volume, 40+x+(sx/2), 66+y, 7);
}

static void drawAgc(int x, int y, int sx)
{
  drawCommon(menu[MENU_AGC_ATT], x, y, sx);

  for(int i=-2 ; i<3 ; i++)
  {
    spr.setTextColor(TH.menu_param, TH.menu_bg);
    spr.fillRoundRect(6+x, 24+y+(2*16), 66+sx, 16, 2, TH.menu_bg);
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
}

static void drawSoftMuteMaxAtt(int x, int y, int sx)
{
  drawCommon(menu[MENU_SOFTMUTE], x, y, sx);

  spr.setTextColor(TH.menu_param, TH.menu_bg);
  spr.fillRoundRect(6+x, 24+y+(2*16), 66+sx, 16, 2, TH.menu_bg);
  spr.drawString("Max Attn", 40+x+(sx/2), 32+y, 2);
  spr.drawNumber(softMuteMaxAttIdx, 40+x+(sx/2), 60+y, 4);
  spr.drawString("dB", 40+x+(sx/2), 90+y, 4);
}

static void drawCal(int x, int y, int sx)
{
  drawCommon(menu[MENU_CALIBRATION], x, y, sx);

  spr.setTextColor(TH.menu_param, TH.menu_bg);
  spr.fillRoundRect(6+x, 24+y+(2*16), 66+sx, 16, 2, TH.menu_bg);
  spr.drawNumber(getCurrentBand()->bandCal, 40+x+(sx/2), 60+y, 4);
  spr.drawString("Hz", 40+x+(sx/2), 90+y, 4);
}

static void drawAvc(int x, int y, int sx)
{
  drawCommon(menu[MENU_AVC], x, y, sx);

  spr.setTextColor(TH.menu_param, TH.menu_bg);
  spr.fillRoundRect(6+x, 24+y+(2*16), 66+sx, 16, 2, TH.menu_bg);
  spr.drawString("Max Gain", 40+x+(sx/2), 32+y, 2);

  // Only show AVC for AM and SSB modes
  if(currentCmd!=FM)
  {
    int currentAvc = isSSB()? SsbAvcIdx : AmAvcIdx;
    spr.drawNumber(currentAvc, 40+x+(sx/2), 60+y, 4);
    spr.drawString("dB", 40+x+(sx/2), 90+y, 4);
  }
}

static void drawBrt(int x, int y, int sx)
{
  drawCommon(settings[MENU_BRIGHTNESS], x, y, sx);

  spr.setTextColor(TH.menu_param, TH.menu_bg);
  spr.fillRoundRect(6+x, 24+y+(2*16), 66+sx, 16, 2, TH.menu_bg);
  spr.drawNumber(currentBrt, 40+x+(sx/2), 60+y, 4);
}

static void drawSleep(int x, int y, int sx)
{
  drawCommon(settings[MENU_SLEEP], x, y, sx);

  spr.setTextColor(TH.menu_param, TH.menu_bg);
  spr.fillRoundRect(6+x, 24+y+(2*16), 66+sx, 16, 2, TH.menu_bg);
  spr.drawNumber(currentSleep, 40+x+(sx/2), 60+y, 4);
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
  if(currentMode==FM)
    spr.drawString(fmStep[fmStepIdx].desc, 48+x, 64+y+(-3*16), 2);
  else
    spr.drawString(amStep[amStepIdx].desc, 48+x, 64+y+(-3*16), 2);

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

  spr.drawString("AVC:", 6+x, 64+y + (0*16), 2);

  if(currentMode==FM)
    sprintf(text, "n/a");
  else if(isSSB())
    sprintf(text, "%2.2ddB", SsbAvcIdx);
  else
    sprintf(text, "%2.2ddB", AmAvcIdx);

  spr.drawString(text, 48+x, 64+y + (0*16), 2);

  /*
    spr.drawString("BFO:", 6+x, 64+y+(2*16), 2);
    if (isSSB()) {
    spr.setTextDatum(MR_DATUM);
    spr.drawString(bfo, 74+x, 64+y+(2*16), 2);
    }
    else spr.drawString("Off", 48+x, 64+y+(2*16), 2);
    spr.setTextDatum(MC_DATUM);
  */

  spr.drawString("Vol:", 6+x, 64+y+(1*16), 2);
  if(muteOn())
  {
    //spr.setTextDatum(MR_DATUM);
    spr.setTextColor(TH.box_off_text, TH.box_off_bg);
    spr.drawString("Muted", 48+x, 64+y+(1*16), 2);
  }
  else
  {
    spr.setTextColor(TH.box_text, TH.box_bg);
    spr.drawNumber(volume, 48+x, 64+y+(1*16), 2);
  }
}

//
// Draw side bar (menu or information)
//
void drawSideBar(uint16_t cmd, int x, int y, int sx)
{
  if(!displayOn()) return;

  switch(cmd)
  {
    case CMD_MENU:      drawMenu(x, y, sx);      break;
    case CMD_SETTINGS:  drawSettings(x, y, sx);  break;
    case CMD_MODE:      drawMode(x, y, sx);      break;
    case CMD_STEP:      drawStep(x, y, sx);      break;
    case CMD_BAND:      drawBand(x, y, sx);      break;
    case CMD_BANDWIDTH: drawBandwidth(x, y, sx); break;
    case CMD_THEME:     drawTheme(x, y, sx);     break;
    case CMD_VOLUME:    drawVolume(x, y, sx);    break;
    case CMD_AGC:       drawAgc(x, y, sx);       break;
    case CMD_SOFTMUTEMAXATT: drawSoftMuteMaxAtt(x, y, sx);break;
    case CMD_CAL:       drawCal(x, y, sx);       break;
    case CMD_AVC:       drawAvc(x, y, sx);       break;
    case CMD_BRT:       drawBrt(x, y, sx);       break;
    case CMD_SLEEP:     drawSleep(x, y, sx);     break;
    default:            drawInfo(x, y, sx);      break;
  }
}
