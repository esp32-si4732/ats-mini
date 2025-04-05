#include "Common.h"
#include "Themes.h"

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
#define MENU_CALIBRATION  9
#define MENU_SETTINGS    10
#if BFO_MENU_EN
#define MENU_BFO         11
#endif

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
  "Calibration",
  "Settings",
#if BFO_MENU_EN
  "BFO",
#endif
};

//
// Settings Menu
//

#define MENU_BRIGHTNESS   0
#define MENU_SLEEP        1
#define MENU_THEME        2
#define MENU_ABOUT        3

int8_t settingsIdx = MENU_BRIGHTNESS;
int8_t currentSettingsCmd = -1;

const char *settings[] =
{
  "Brightness",
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

int idxFmStep = 1;
Step fmStep[] =
{
  {   5, "50k"  }, 
  {  10, "100k" },
  {  20, "200k" },
  { 100, "1M"   },  
};

volatile int8_t idxAmStep = 3;
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

//
// Bands Menu
//
// YOU CAN CONFIGURE YOUR OWN BAND PLAN. Be guided by the comments.
// To add a new band, all you have to do is insert a new line in the table below and adjust the bandCAL and bandMODE size.
// No extra code will be needed. You can remove a band by deleting a line if you do not want a given band.
// Also, you can change the parameters of the band.
// ATTENTION: You have to RESET the eeprom after adding or removing a line of this table.
//            Turn your receiver on with the encoder push button pressed at first time to RESET the eeprom content.
//

int bandIdx = 0;

Band band[] =
{
  {"VHF", FM_BAND_TYPE, 6400, 10800, 10390, 1, 0},
  {"MW1", MW_BAND_TYPE, 150, 1720, 810, 3, 4},
  {"MW2", MW_BAND_TYPE, 531, 1701, 783, 2, 4},
  {"MW3", MW_BAND_TYPE, 1700, 3500, 2500, 1, 4},
  {"80M", MW_BAND_TYPE, 3500, 4000, 3700, 0, 4},
  {"SW1", SW_BAND_TYPE, 4000, 5500, 4885, 1, 4},
  {"SW2", SW_BAND_TYPE, 5500, 6500, 6000, 1, 4},
  {"40M", SW_BAND_TYPE, 6500, 7300, 7100, 0, 4},
  {"SW3", SW_BAND_TYPE, 7200, 8000, 7200, 1, 4},
  {"SW4", SW_BAND_TYPE, 9000, 11000, 9500, 1, 4},
  {"SW5", SW_BAND_TYPE, 11100, 13000, 11900, 1, 4},
  {"SW6", SW_BAND_TYPE, 13000, 14000, 13500, 1, 4},
  {"20M", SW_BAND_TYPE, 14000, 15000, 14200, 0, 4},
  {"SW7", SW_BAND_TYPE, 15000, 17000, 15300, 1, 4},
  {"SW8", SW_BAND_TYPE, 17000, 18000, 17500, 1, 4},
  {"15M", SW_BAND_TYPE, 20000, 21400, 21100, 0, 4},
  {"SW9", SW_BAND_TYPE, 21400, 22800, 21500, 1, 4},
  {"CB ", SW_BAND_TYPE, 26000, 30000, 27135, 0, 4},
  {"10M", SW_BAND_TYPE, 28000, 30000, 28400, 0, 4},
  {"ALL", SW_BAND_TYPE, 150, 30000, 15000, 0, 4} // All band. LW, MW and SW (from 150kHz to 30MHz)
};

//
// Utility functions to change menu values
//

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

static void doSettings(int dir)
{
  settingsIdx = wrap_range(settingsIdx, dir, 0, LAST_ITEM(settings));
}

static void doVolume(int dir)
{
  if(dir>0) rx.volumeUp(); else if(dir<0) rx.volumeDown();
}

static void doTheme(int dir)
{
  themeIdx = wrap_range(themeIdx, dir, 0, LAST_ITEM(theme));
}

void doAvc(int dir)
{
  // Only allow for AM and SSB modes
  if(currentCmd==FM) return;

  if(isSSB())
    currentAVC = SsbAvcIdx = wrap_range(SsbAvcIdx, 2*dir, 12, 90);
  else
    currentAVC = AmAvcIdx = wrap_range(AmAvcIdx, 2*dir, 12, 90);

  // Configure SI4732/5
  rx.setAvcAmMaxGain(currentAVC);
}

static void doCal(int dir)
{
  bandCAL[bandIdx] = currentCAL = clamp_range(bandCAL[bandIdx], 10*dir, -CALMax, CALMax);

  // If in SSB mode set the SI4732/5 BFO value
  // This adjusts the BFO while in the calibration menu
  if(isSSB()) updateBFO();
}

static void doBrt(int dir)
{
  currentBrt = clamp_range(currentBrt, 32*dir, 32, 255);

  if(display_on) ledcWrite(PIN_LCD_BL, currentBrt);
}

static void doSleep(int dir)
{
  currentSleep = clamp_range(currentSleep, 5*dir, 0, 255);
}

static void doStep(int dir)
{
  if(currentMode==FM)
  {
    currentStepIdx = idxFmStep = wrap_range(idxFmStep, dir, 0, LAST_ITEM(fmStep));
    rx.setFrequencyStep(fmStep[currentStepIdx].step);
  }
  else
  {
    int lastAmStep = getLastStep();
    idxAmStep = wrap_range(idxAmStep, dir, 0, lastAmStep);

    // SSB step limit
    if(isSSB() && idxAmStep>=AmTotalStepsSsb && idxAmStep<AmTotalSteps)
      idxAmStep = dir>0? AmTotalSteps : AmTotalStepsSsb-1;

    // ?????
    if(!isSSB() || (idxAmStep<AmTotalSteps))
    {
      currentStepIdx = idxAmStep;
      rx.setFrequencyStep(amStep[idxAmStep].step);
    }

    currentStepIdx = idxAmStep;

    // Max 10kHz for spacing
    rx.setSeekAmSpacing(5);
  }

  // Debug
#if DEBUG2_PRINT
  int lastStep = getLastStep();
  Serial.print("Info: doStep() >>> currentStepIdx = ");
  Serial.print(currentStepIdx);
  Serial.print(", getLastStep() = ");
  Serial.println(lastStep);
#endif

  band[bandIdx].currentStepIdx = currentStepIdx;
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

static void doMode(int dir)
{
  // This is our current mode
  currentMode = bandMODE[bandIdx];

  // Nothing to do if FM mode
  if(currentMode==FM) return;

  // Clockwise
  if(dir>0)
  {
    switch(currentMode)
    {
      case AM:
        // When switching from AM mode, load SSB patch!
        drawLoadingSSB();
        loadSSB();
        ssbLoaded = true;
        currentMode = LSB;
        break;
      case LSB:
        currentMode = USB;
        break;
      case USB:
        // When exiting SSB mode, update current frequency and BFO
        currentFrequency = currentFrequency + (currentBFO / 1000);
        currentBFO = 0;
        currentMode = AM;
        bfoOn = ssbLoaded = false;
        break;
    }
  }
  // Counterclockwise
  else if(dir<0)
  {
    switch(currentMode)
    {
      case AM:
        // If switching from AM mode, load SSB patch!
        drawLoadingSSB();
        loadSSB();
        ssbLoaded = true;
        currentMode = USB;
        break;
      case USB:
        currentMode = LSB;
        break;
      case LSB:
        // When exiting SSB mode, update current frequency and BFO
        currentFrequency = currentFrequency + (currentBFO / 1000);
        currentBFO = 0;
        currentMode = AM;
        bfoOn = ssbLoaded = false;
        break;
    }
  }

  band[bandIdx].currentFreq = currentFrequency;
  band[bandIdx].currentStepIdx = currentStepIdx;
  bandMODE[bandIdx] = currentMode;
  useBand();
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

static void doBand(int dir)
{
  // G8PTN: Reset BFO when changing band and store frequency
  band[bandIdx].currentFreq = currentFrequency + (currentBFO / 1000);
  band[bandIdx].currentStepIdx = currentStepIdx;
  currentBFO = 0;

  // Change band
  bandIdx = wrap_range(bandIdx, dir, 0, lastBand);
  currentMode = bandMODE[bandIdx];

  // Load SSB patch as required
  if(!isSSB())
    ssbLoaded = false;
  else if(!ssbLoaded)
  {
    drawLoadingSSB();
    loadSSB();
    ssbLoaded = true;
  }

  // Set new band
  useBand();
}

static void doBandwidth(int dir)
{
  if(isSSB())
  {
    bwIdxSSB = wrap_range(bwIdxSSB, dir, 0, LAST_ITEM(bandwidthSSB));
    rx.setSSBAudioBandwidth(bandwidthSSB[bwIdxSSB].idx);
    correctCutoffFilter();
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
  if(!dir) return(true);

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
    case CMD_CAL:       doCal(dir);break;
    case CMD_AVC:       doAvc(dir);break;
    case CMD_SETTINGS:  doSettings(dir);break;
    case CMD_BRT:       doBrt(dir);break;
    case CMD_SLEEP:     doSleep(dir);break;
    case CMD_THEME:     doTheme(dir);break;
    case CMD_ABOUT:     return(true);
    default:            return(false);
  }

  // Redraw screen
  drawScreen(currentCmd);

  // Encoder input handled
  return(true);
}


//
// Draw functions
//

static void drawCommon(uint16_t cmd, int x, int y, int sx)
{
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(theme[themeIdx].menu_hdr,theme[themeIdx].menu_bg);
  spr.fillSmoothRoundRect(1+x,1+y,76+sx,110,4,theme[themeIdx].menu_border);
  spr.fillSmoothRoundRect(2+x,2+y,74+sx,108,4,theme[themeIdx].menu_bg);
  
  if(isSettingsMode(cmd)) 
    spr.drawString(settings[settingsIdx],40+x+(sx/2),12+y,2);
  else
    spr.drawString(menu[menuIdx],40+x+(sx/2),12+y,2);

  spr.drawLine(1+x, 23+y, 76+sx, 23+y, theme[themeIdx].menu_border);

  spr.setTextFont(0);
  spr.setTextColor(theme[themeIdx].menu_item,theme[themeIdx].menu_bg);
  spr.fillRoundRect(6+x,24+y+(2*16),66+sx,16,2,theme[themeIdx].menu_hl_bg);
}

static void drawMenu(int x, int y, int sx)
{
  spr.setTextDatum(MC_DATUM);

  spr.fillSmoothRoundRect(1+x,1+y,76+sx,110,4,theme[themeIdx].menu_border);
  spr.fillSmoothRoundRect(2+x,2+y,74+sx,108,4,theme[themeIdx].menu_bg);
  spr.setTextColor(theme[themeIdx].menu_hdr, theme[themeIdx].menu_bg);

  char label_menu[16];
  sprintf(label_menu, "Menu %2.2d/%2.2d", (menuIdx + 1), (lastMenu + 1));
  //spr.drawString("Menu",38+x+(sx/2),14+y,2);
  spr.drawString(label_menu,40+x+(sx/2),12+y,2);
  spr.drawLine(1+x, 23+y, 76+sx, 23+y, theme[themeIdx].menu_border);

  spr.setTextFont(0);
  spr.setTextColor(theme[themeIdx].menu_item, theme[themeIdx].menu_bg);
  spr.fillRoundRect(6+x,24+y+(2*16),66+sx,16,2,theme[themeIdx].menu_hl_bg);

  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0)
      spr.setTextColor(theme[themeIdx].menu_hl_text,theme[themeIdx].menu_hl_bg);
    else
      spr.setTextColor(theme[themeIdx].menu_item,theme[themeIdx].menu_bg);

    spr.drawString(menu[abs((menuIdx+lastMenu+1+i)%(lastMenu+1))],40+x+(sx/2),64+y+(i*16),2);
  }
}

static void drawSettings(int x, int y, int sx)
{
  spr.setTextDatum(MC_DATUM);

  spr.fillSmoothRoundRect(1+x,1+y,76+sx,110,4,theme[themeIdx].menu_border);
  spr.fillSmoothRoundRect(2+x,2+y,74+sx,108,4,theme[themeIdx].menu_bg);
  spr.setTextColor(theme[themeIdx].menu_hdr, theme[themeIdx].menu_bg);
  spr.drawString("Settings",40+x+(sx/2),12+y,2);
  spr.drawLine(1+x, 23+y, 76+sx, 23+y, theme[themeIdx].menu_border);

  spr.setTextFont(0);
  spr.setTextColor(theme[themeIdx].menu_item,theme[themeIdx].menu_bg);
  spr.fillRoundRect(6+x,24+y+(2*16),66+sx,16,2,theme[themeIdx].menu_hl_bg);

  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0)
      spr.setTextColor(theme[themeIdx].menu_hl_text,theme[themeIdx].menu_hl_bg);
    else
      spr.setTextColor(theme[themeIdx].menu_item,theme[themeIdx].menu_bg);

    spr.drawString(settings[abs((settingsIdx+lastSettingsMenu+1+i)%(lastSettingsMenu+1))],40+x+(sx/2),64+y+(i*16),2);
  }
}

static void drawMode(int x, int y, int sx)
{
  drawCommon(CMD_MODE, x, y, sx);

  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0)
      spr.setTextColor(theme[themeIdx].menu_hl_text,theme[themeIdx].menu_hl_bg);
    else
      spr.setTextColor(theme[themeIdx].menu_item,theme[themeIdx].menu_bg);

    if((currentMode!=FM) || (i==0))
    {
      int count = ITEM_COUNT(bandModeDesc);
      spr.drawString(bandModeDesc[abs((currentMode+count+i)%count)],40+x+(sx/2),64+y+(i*16),2);
    }
  }
}

static void drawStep(int x, int y, int sx)
{
  int lastStep = getLastStep();

  drawCommon(CMD_STEP, x, y, sx);

  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0)
      spr.setTextColor(theme[themeIdx].menu_hl_text,theme[themeIdx].menu_hl_bg);
    else
      spr.setTextColor(theme[themeIdx].menu_item,theme[themeIdx].menu_bg);

    if(currentMode==FM)
      spr.drawString(fmStep[abs((currentStepIdx+lastFmStep+1+i)%(lastFmStep+1))].desc,40+x+(sx/2),64+y+(i*16),2);
    else
      spr.drawString(amStep[abs((currentStepIdx+lastStep+1+i)%(lastStep+1))].desc,40+x+(sx/2),64+y+(i*16),2);
  }
}

static void drawBand(int x, int y, int sx)
{
  drawCommon(CMD_BAND, x, y, sx);

  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0)
      spr.setTextColor(theme[themeIdx].menu_hl_text,theme[themeIdx].menu_hl_bg);
    else
      spr.setTextColor(theme[themeIdx].menu_item,theme[themeIdx].menu_bg);

    spr.drawString(band[abs((bandIdx+lastBand+1+i)%(lastBand+1))].bandName,40+x+(sx/2),64+y+(i*16),2);
  }
}

static void drawBandwidth(int x, int y, int sx)
{
  drawCommon(CMD_BANDWIDTH, x, y, sx);

  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0)
      spr.setTextColor(theme[themeIdx].menu_hl_text,theme[themeIdx].menu_hl_bg);
    else
      spr.setTextColor(theme[themeIdx].menu_item,theme[themeIdx].menu_bg);

    if(isSSB())
    {
      int count = ITEM_COUNT(bandwidthSSB);
      spr.drawString(bandwidthSSB[abs((bwIdxSSB+count+i)%count)].desc,40+x+(sx/2),64+y+(i*16),2);
    }
    else if(currentMode==FM)
    {
      int count = ITEM_COUNT(bandwidthFM);
      spr.drawString(bandwidthFM[abs((bwIdxFM+count+i)%count)].desc,40+x+(sx/2),64+y+(i*16),2);
    }
    else
    {
      int count = ITEM_COUNT(bandwidthAM);
      spr.drawString(bandwidthAM[abs((bwIdxAM+count+i)%count)].desc,40+x+(sx/2),64+y+(i*16),2);
     }
  }
}

static void drawTheme(int x, int y, int sx)
{
  drawCommon(CMD_THEME, x, y, sx);

  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0)
      spr.setTextColor(theme[themeIdx].menu_hl_text,theme[themeIdx].menu_hl_bg);
    else
      spr.setTextColor(theme[themeIdx].menu_item,theme[themeIdx].menu_bg);

    int count = ITEM_COUNT(theme);
    spr.drawString(theme[abs((themeIdx+count+i)%count)].name, 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawVolume(int x, int y, int sx)
{
  drawCommon(CMD_VOLUME, x, y, sx);

  for(int i=-2 ; i<3 ; i++)
  {
    spr.setTextColor(theme[themeIdx].menu_param,theme[themeIdx].menu_bg);
    spr.fillRoundRect(6+x,24+y+(2*16),66+sx,16,2,theme[themeIdx].menu_bg);
    spr.drawNumber(rx.getVolume(),40+x+(sx/2),66+y,7);
  }
}

static void drawAgc(int x, int y, int sx)
{
  drawCommon(CMD_AGC, x, y, sx);

  for(int i=-2 ; i<3 ; i++)
  {
    spr.setTextColor(theme[themeIdx].menu_param,theme[themeIdx].menu_bg);
    spr.fillRoundRect(6+x,24+y+(2*16),66+sx,16,2,theme[themeIdx].menu_bg);
    // G8PTN: Read back value is not used
    // rx.getAutomaticGainControl();
    if(!agcNdx && !agcIdx)
    {
      spr.setFreeFont(&Orbitron_Light_24);
      spr.drawString("AGC",40+x+(sx/2),48+y);
      spr.drawString("On",40+x+(sx/2),72+y);
      spr.setTextFont(0);
    }
    else
    {
      char text[16];
      sprintf(text, "%2.2d", agcNdx);
      spr.drawString(text,40+x+(sx/2),60+y,7);
    }
  }
}

static void drawSoftMuteMaxAtt(int x, int y, int sx)
{
  drawCommon(CMD_SOFTMUTEMAXATT, x, y, sx);

  for(int i=-2 ; i<3 ; i++)
  {
    spr.setTextColor(theme[themeIdx].menu_param,theme[themeIdx].menu_bg);
    spr.fillRoundRect(6+x,24+y+(2*16),66+sx,16,2,theme[themeIdx].menu_bg);
    spr.drawString("Max Attn",40+x+(sx/2),32+y,2);
    spr.drawNumber(softMuteMaxAttIdx,40+x+(sx/2),60+y,4);
    spr.drawString("dB",40+x+(sx/2),90+y,4);
  }
}

static void drawCal(int x, int y, int sx)
{
  drawCommon(CMD_CAL, x, y, sx);

  for(int i=-2 ; i<3 ; i++)
  {
    spr.setTextColor(theme[themeIdx].menu_param,theme[themeIdx].menu_bg);
    spr.fillRoundRect(6+x,24+y+(2*16),66+sx,16,2,theme[themeIdx].menu_bg);
    spr.drawNumber(currentCAL,40+x+(sx/2),60+y,4);
    spr.drawString("Hz",40+x+(sx/2),90+y,4);
  }
}

static void drawAvc(int x, int y, int sx)
{
  drawCommon(CMD_AVC, x, y, sx);

  for(int i=-2 ; i<3 ; i++)
  {
    spr.setTextColor(theme[themeIdx].menu_param,theme[themeIdx].menu_bg);
    spr.fillRoundRect(6+x,24+y+(2*16),66+sx,16,2,theme[themeIdx].menu_bg);
    spr.drawString("Max Gain",40+x+(sx/2),32+y,2);
    spr.drawNumber(currentAVC,40+x+(sx/2),60+y,4);
    spr.drawString("dB",40+x+(sx/2),90+y,4);
  }
}

static void drawBrt(int x, int y, int sx)
{
  drawCommon(CMD_BRT, x, y, sx);

  for(int i=-2 ; i<3 ; i++)
  {
    spr.setTextColor(theme[themeIdx].menu_param,theme[themeIdx].menu_bg);
    spr.fillRoundRect(6+x,24+y+(2*16),66+sx,16,2,theme[themeIdx].menu_bg);
    spr.drawNumber(currentBrt,40+x+(sx/2),60+y,4);
  }
}

static void drawSleep(int x, int y, int sx)
{
  drawCommon(CMD_SLEEP, x, y, sx);

  for(int i=-2 ; i<3 ; i++)
  {
    spr.setTextColor(theme[themeIdx].menu_param,theme[themeIdx].menu_bg);
    spr.fillRoundRect(6+x,24+y+(2*16),66+sx,16,2,theme[themeIdx].menu_bg);
    spr.drawNumber(currentSleep,40+x+(sx/2),60+y,4);
  }
}

static void drawInfo(int x, int y, int sx)
{
  char text[16];

  // Info box
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(theme[themeIdx].box_text, theme[themeIdx].box_bg);
  spr.fillSmoothRoundRect(1+x, 1+y, 76+sx, 110, 4, theme[themeIdx].box_border);
  spr.fillSmoothRoundRect(2+x, 2+y, 74+sx, 108, 4, theme[themeIdx].box_bg);
  spr.drawString("Step:",6+x,64+y+(-3*16),2);

  if(currentMode==FM)
    spr.drawString(fmStep[currentStepIdx].desc,48+x,64+y+(-3*16),2);
  else
    spr.drawString(amStep[currentStepIdx].desc,48+x,64+y+(-3*16),2);

  spr.drawString("BW:",6+x,64+y+(-2*16),2);

  if(isSSB())
    spr.drawString(bandwidthSSB[bwIdxSSB].desc,48+x,64+y+(-2*16),2);
  else if(currentMode==FM)
    spr.drawString(bandwidthFM[bwIdxFM].desc,48+x,64+y+(-2*16),2);
  else
    spr.drawString(bandwidthAM[bwIdxAM].desc,48+x,64+y+(-2*16),2);

  if(!agcNdx && !agcIdx)
  {
    spr.drawString("AGC:",6+x,64+y+(-1*16),2);
    spr.drawString("On",48+x,64+y+(-1*16),2);
  }
  else
  {
    sprintf(text, "%2.2d", agcNdx);
    spr.drawString("Att:",6+x,64+y+(-1*16),2);
    spr.drawString(text,48+x,64+y+(-1*16),2);
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
    spr.drawString("BFO:",6+x,64+y+(2*16),2);
    if (isSSB()) {
    spr.setTextDatum(MR_DATUM);
    spr.drawString(bfo,74+x,64+y+(2*16),2);
    }
    else spr.drawString("Off",48+x,64+y+(2*16),2);
    spr.setTextDatum(MC_DATUM);
  */

  spr.drawString("Vol:",6+x,64+y+(1*16),2);
  if(muted)
  {
    //spr.setTextDatum(MR_DATUM);
    spr.setTextColor(theme[themeIdx].box_off_text, theme[themeIdx].box_off_bg);
    spr.drawString("Muted",48+x,64+y+(1*16),2);
  }
  else
  {
    spr.setTextColor(theme[themeIdx].box_text, theme[themeIdx].box_bg);
    spr.drawNumber(rx.getVolume(),48+x,64+y+(1*16),2);
  }
}

//
// Draw side bar (menu or information)
//
void drawSideBar(uint16_t cmd, int x, int y, int sx)
{
  if(!display_on) return;

  switch(cmd)
  {
    case CMD_MENU:      drawMenu(x, y, sx);      break;
    case CMD_SETTINGS:  drawSettings(x, y, sx);  break;
    case CMD_MODE:      drawMode(x, y, sx);      break;
    case CMD_STEP:      drawStep(x, y, sx);      break;
    case CMD_BAND:      drawStep(x, y, sx);      break;
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
