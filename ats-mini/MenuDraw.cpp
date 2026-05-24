#include "Common.h"
#include "Themes.h"
#include "MenuData.h"
#include "Utils.h"
#include "Draw.h"
#include "Menu.h"
#include "MenuDraw.h"

static void drawCommon(const char *title, int x, int y, int sx, bool cursor = false)
{
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_hdr);
  spr.fillSmoothRoundRect(1+x, 1+y, 76+sx, 110, 4, TH.menu_border);
  spr.fillSmoothRoundRect(2+x, 2+y, 74+sx, 108, 4, TH.menu_bg);

  spr.drawString(title, 40+x+(sx/2), 12+y, 2);
  spr.drawLine(1+x, 23+y, 76+sx, 23+y, TH.menu_border);

  spr.setTextFont(0);
  spr.setTextColor(TH.menu_item);
  if(cursor)
    spr.fillRoundRect(6+x, 24+y+(2*16), 66+sx, 16, 2, TH.menu_hl_bg);
}

static void drawMenu(int x, int y, int sx)
{
  spr.setTextDatum(MC_DATUM);

  spr.fillSmoothRoundRect(1+x, 1+y, 76+sx, 110, 4, TH.menu_border);
  spr.fillSmoothRoundRect(2+x, 2+y, 74+sx, 108, 4, TH.menu_bg);
  spr.setTextColor(TH.menu_hdr);

  spr.drawString("Menu", 40+x+(sx/2), 12+y, 2);
  spr.drawLine(1+x, 23+y, 76+sx, 23+y, TH.menu_border);

  spr.setTextFont(0);
  spr.setTextColor(TH.menu_item);
  spr.fillRoundRect(6+x, 24+y+(2*16), 66+sx, 16, 2, TH.menu_hl_bg);

  int count = getTotalMenuItems();
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(menu[abs((menuIdx+count+i)%count)]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
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
  spr.setTextColor(TH.menu_hdr);
  spr.drawString("Settings", 40+x+(sx/2), 12+y, 2);
  spr.drawLine(1+x, 23+y, 76+sx, 23+y, TH.menu_border);

  spr.setTextFont(0);
  spr.setTextColor(TH.menu_item);
  spr.fillRoundRect(6+x, 24+y+(2*16), 66+sx, 16, 2, TH.menu_hl_bg);

  int count = getTotalSettingsItems();
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(settings[abs((settingsIdx+count+i)%count)]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(settings[abs((settingsIdx+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawMode(int x, int y, int sx)
{
  drawCommon(menu[MENU_MODE], x, y, sx, true);

  int count = getTotalModes();
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(bandModeDesc[abs((radioState.mode+count+i)%count)]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    spr.setTextDatum(MC_DATUM);
    if((radioState.mode!=FM) || (i==0))
     spr.drawString(bandModeDesc[abs((radioState.mode+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawStep(int x, int y, int sx)
{
  int count = getLastStep(radioState.mode) + 1;
  int idx   = bands[bandIdx].currentStepIdx + count;

  drawCommon(menu[MENU_STEP], x, y, sx, true);

  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(steps[radioState.mode][abs((idx+i)%count)].desc);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(steps[radioState.mode][abs((idx+i)%count)].desc, 40+x+(sx/2), 64+y+(i*16), 2);
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

static void drawScan(int x, int y, int sx)
{
  drawCommon(menu[MENU_SCAN], x, y, sx);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TH.scan_rssi);
  spr.drawString("S", 40+x+(sx/2)-30, 66+y+30, 2);
  spr.setTextColor(TH.scan_snr);
  spr.drawString("N", 40+x+(sx/2)+30, 66+y+30, 2);

  spr.drawSmoothArc(40+x+(sx/2), 66+y, 30, 27, 45, 180, TH.menu_param, TH.menu_bg);
  spr.fillTriangle(40+x+(sx/2)-5, 66+y-32, 40+x+(sx/2)+5, 66+y-27, 40+x+(sx/2)-5, 66+y-22, TH.menu_param);
  spr.drawSmoothArc(40+x+(sx/2), 66+y, 30, 27, 225, 360, TH.menu_param, TH.menu_bg);
  spr.fillTriangle(40+x+(sx/2)+5, 66+y+32, 40+x+(sx/2)-5, 66+y+27, 40+x+(sx/2)+5, 66+y+22, TH.menu_param);

  spr.drawLine(40+x+(sx/2)-17, 66+y+5, 40+x+(sx/2)-4, 66+y+5, TH.menu_param);
  spr.drawLine(40+x+(sx/2)-4, 66+y+5, 40+x+(sx/2), 66+y-16+5, TH.menu_param);
  spr.drawLine(40+x+(sx/2), 66+y-16+5, 40+x+(sx/2)+4, 66+y+5, TH.menu_param);
  spr.drawLine(40+x+(sx/2)+4, 66+y+5, 40+x+(sx/2)+17, 66+y+5, TH.menu_param);
}

static void drawBand(int x, int y, int sx)
{
  drawCommon(menu[MENU_BAND], x, y, sx, true);

  int count = getTotalBands();
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(bands[abs((bandIdx+count+i)%count)].bandName);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(bands[abs((bandIdx+count+i)%count)].bandName, 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawBandwidth(int x, int y, int sx)
{
  int count = getLastBandwidth(radioState.mode) + 1;
  int idx   = bands[bandIdx].bandwidthIdx + count;

  drawCommon(menu[MENU_BW], x, y, sx, true);

  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(bandwidths[radioState.mode][abs((idx+i)%count)].desc);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(bandwidths[radioState.mode][abs((idx+i)%count)].desc, 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawSleepMode(int x, int y, int sx)
{
  drawCommon(settings[MENU_SLEEPMODE], x, y, sx, true);

  int count = getTotalSleepModes();
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(sleepModeDesc[abs((radioState.sleepMode+count+i)%count)]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(sleepModeDesc[abs((radioState.sleepMode+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawUSBMode(int x, int y, int sx)
{
  drawCommon(settings[MENU_USBMODE], x, y, sx, true);

  int count = getTotalUSBModes();
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(usbModeDesc[abs((radioState.usbMode+count+i)%count)]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    // Prevent repeats for short menus
    if (count < 5 && ((radioState.usbMode+i) < 0 || (radioState.usbMode+i) >= count)) {
      continue;
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(usbModeDesc[abs((radioState.usbMode+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawBleMode(int x, int y, int sx)
{
  drawCommon(settings[MENU_BLEMODE], x, y, sx, true);

  int count = getTotalBleModes();
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(bleModeDesc[abs((radioState.bleMode+count+i)%count)]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    // Prevent repeats for short menus
    if (count < 5 && ((radioState.bleMode+i) < 0 || (radioState.bleMode+i) >= count)) {
      continue;
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(bleModeDesc[abs((radioState.bleMode+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawWiFiMode(int x, int y, int sx)
{
  drawCommon(settings[MENU_WIFIMODE], x, y, sx, true);

  int count = getTotalWiFiModes();
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(wifiModeDesc[abs((radioState.wifiMode+count+i)%count)]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(wifiModeDesc[abs((radioState.wifiMode+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), 2);
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
      spr.setTextColor(TH.menu_item);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(theme[abs((themeIdx+count+i)%count)].name, 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawUILayout(int x, int y, int sx)
{
  drawCommon(settings[MENU_UI], x, y, sx, true);

  int count = getTotalUILayouts();
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(uiLayoutDesc[abs((radioState.uiLayout+count+i)%count)]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    // Prevent repeats for short menus
    if (count < 5 && ((radioState.uiLayout+i) < 0 || (radioState.uiLayout+i) >= count)) {
      continue;
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(uiLayoutDesc[abs((radioState.uiLayout+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawRDSMode(int x, int y, int sx)
{
  drawCommon(settings[MENU_RDS], x, y, sx, true);

  int count = getTotalRDSModes();
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(rdsMode[abs((radioState.rdsMode+count+i)%count)].desc);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(rdsMode[abs((radioState.rdsMode+count+i)%count)].desc, 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawUTCOffset(int x, int y, int sx)
{
  drawCommon(settings[MENU_UTCOFFSET], x, y, sx, true);

  int count = getTotalUTCOffsets();
  uint8_t idx = radioState.utcOffset;

  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0)
    {
      drawZoomedMenu(utcOffsets[abs((idx+count+i)%count)].desc);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    }
    else
    {
      spr.setTextColor(TH.menu_item);
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

  int count = getTotalMemories();
  for(int i=-2 ; i<3 ; i++)
  {
    int j = abs((memoryIdx+count+i)%count);
    char buf[16];
    const char *text = buf;

    if(!memories[j].freq)
      text = "- - -";
    else if(memories[j].mode==FM)
      sprintf(buf, "%3.2f %s", memories[j].freq / 1000000.0, bandModeDesc[memories[j].mode]);
    else
      sprintf(buf, "%5lu %s", memories[j].freq / 1000, bandModeDesc[memories[j].mode]);

    if(i==0) {
      drawZoomedMenu(text);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
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

  spr.setTextColor(TH.menu_param);
  spr.drawNumber(radioState.vol, 40+x+(sx/2), 66+y, 7);

  if(muteOn(MUTE_MAIN))
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
  spr.setTextColor(TH.menu_param);

  // G8PTN: Read back value is not used
  // rx.getAutomaticGainControl();
  if(!radioState.agcNdxVal && !radioState.agcIndex)
  {
    spr.setFreeFont(&Orbitron_Light_24);
    spr.drawString("AGC", 40+x+(sx/2), 48+y);
    spr.drawString("On", 40+x+(sx/2), 72+y);
    spr.setTextFont(0);
  }
  else
  {
    char text[16];
    sprintf(text, "%2.2d", radioState.agcNdxVal);
    spr.drawString(text, 40+x+(sx/2), 60+y, 7);
  }
}

static void drawSquelch(int x, int y, int sx)
{
  drawCommon(menu[MENU_SQUELCH], x, y, sx);
  drawZoomedMenu(menu[MENU_SQUELCH]);
  spr.setTextDatum(MC_DATUM);

  uint8_t squelchValue = radioState.squelch[radioState.mode] & 0x7f;
  bool squelchParam = radioState.squelch[radioState.mode] & 0x80;
  if(squelchValue)
  {
    spr.drawNumber(squelchValue, 40+x+(sx/2), 60+y, 4);
    spr.drawString(squelchParam? "dB":"dBuV", 40+x+(sx/2), 90+y, 4);
  }
  else
  {
    spr.drawString("Off", 40+x+(sx/2), 60+y, 4);
    spr.drawString(squelchParam? "(snr)":"(rssi)", 40+x+(sx/2), 90+y, 4);
  }
}

static void drawSoftMuteMaxAtt(int x, int y, int sx)
{
  drawCommon(menu[MENU_SOFTMUTE], x, y, sx);
  drawZoomedMenu(menu[MENU_SOFTMUTE]);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_param);
  spr.drawString("Max Attn", 40+x+(sx/2), 32+y, 2);
  spr.drawNumber(radioState.softMuteMaxAtt, 40+x+(sx/2), 60+y, 4);
  spr.drawString("dB", 40+x+(sx/2), 90+y, 4);
}

static void drawCal(int x, int y, int sx)
{
  drawCommon(settings[MENU_CALIBRATION], x, y, sx);
  drawZoomedMenu(settings[MENU_CALIBRATION]);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_param);
  if (radioState.mode == USB)
  {
    spr.drawString("USB", 40+x+(sx/2), 35+y, 2);
    spr.drawNumber(getCurrentBand()->usbCal, 40+x+(sx/2), 65+y, 4);
  }
  else if (radioState.mode == LSB)
  {
    spr.drawString("LSB", 40+x+(sx/2), 35+y, 2);
    spr.drawNumber(getCurrentBand()->lsbCal, 40+x+(sx/2), 65+y, 4);
  }
  else
    spr.drawNumber(0, 40+x+(sx/2), 65+y, 4);  // Display zero or nothing for other modes

  spr.drawString("Hz", 40+x+(sx/2), 95+y, 4);
}

static void drawAvc(int x, int y, int sx)
{
  drawCommon(menu[MENU_AVC], x, y, sx);
  drawZoomedMenu(menu[MENU_AVC]);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_param);
  spr.drawString("Max Gain", 40+x+(sx/2), 32+y, 2);

  // Only show AVC for AM and SSB modes
  if(radioState.mode!=FM)
  {
    int currentAvc = isSSB()? radioState.ssbAvcIdx : radioState.amAvcIdx;
    spr.drawNumber(currentAvc, 40+x+(sx/2), 60+y, 4);
    spr.drawString("dB", 40+x+(sx/2), 90+y, 4);
  }
}

static void drawFmRegion(int x, int y, int sx)
{
  drawCommon(settings[MENU_FM_REGION], x, y, sx, true);

  int count = getTotalFmRegions();
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(fmRegions[abs((radioState.fmRegionIdx+count+i)%count)].desc);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    // Prevent repeats for short menus
    if (count < 5 && ((radioState.fmRegionIdx+i) < 0 || (radioState.fmRegionIdx+i) >= count)) {
      continue;
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(fmRegions[abs((radioState.fmRegionIdx+count+i)%count)].desc, 40+x+(sx/2), 64+y+(i*16), 2);
  }
}

static void drawBrt(int x, int y, int sx)
{
  drawCommon(settings[MENU_BRIGHTNESS], x, y, sx);
  drawZoomedMenu(settings[MENU_BRIGHTNESS]);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_param);
  spr.drawNumber(radioState.brightness, 40+x+(sx/2), 60+y, 4);
}

static void drawSleep(int x, int y, int sx)
{
  drawCommon(settings[MENU_SLEEP], x, y, sx);
  drawZoomedMenu(settings[MENU_SLEEP]);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_param);
  spr.drawNumber(radioState.sleep, 40+x+(sx/2), 60+y, 4);
}

static void drawZoom(int x, int y, int sx)
{
  drawCommon(settings[MENU_ZOOM], x, y, sx);
  drawZoomedMenu(settings[MENU_ZOOM]);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_param);
  spr.drawString(radioState.zoomLevel ? "On" : "Off", 40+x+(sx/2), 60+y, 4);
}

static void drawScrollDir(int x, int y, int sx)
{
  drawCommon(settings[MENU_SCROLL], x, y, sx);
  drawZoomedMenu(settings[MENU_SCROLL]);

  spr.fillRect(37+x+(sx/2), 45+y, 5, 40, TH.menu_param);
  if(radioState.scrollDir>0)
    spr.fillTriangle(39+x+(sx/2)-5, 45+y, 39+x+(sx/2)+5, 45+y, 39+x+(sx/2), 45+y-5, TH.menu_param);
  else
    spr.fillTriangle(39+x+(sx/2)-5, 85+y, 39+x+(sx/2)+5, 85+y, 39+x+(sx/2), 85+y+5, TH.menu_param);
}

static void drawInfo(int x, int y, int sx)
{
  char text[16];

  // Info box
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(TH.box_text);
  spr.fillSmoothRoundRect(1+x, 1+y, 76+sx, 110, 4, TH.box_border);
  spr.fillSmoothRoundRect(2+x, 2+y, 74+sx, 108, 4, TH.box_bg);

  spr.drawString("Step:", 6+x, 64+y+(-3*16), 2);
  spr.drawString(getCurrentStep()->desc, 48+x, 64+y+(-3*16), 2);

  spr.drawString("BW:", 6+x, 64+y+(-2*16), 2);
  spr.drawString(getCurrentBandwidth()->desc, 48+x, 64+y+(-2*16), 2);

  if(!radioState.agcNdxVal && !radioState.agcIndex)
  {
    spr.drawString("AGC:", 6+x, 64+y+(-1*16), 2);
    spr.drawString("On", 48+x, 64+y+(-1*16), 2);
  }
  else
  {
    sprintf(text, "%2.2d", radioState.agcNdxVal);
    spr.drawString("Att:", 6+x, 64+y+(-1*16), 2);
    spr.drawString(text, 48+x, 64+y+(-1*16), 2);
  }

  spr.drawString("Vol:", 6+x, 64+y+(0*16), 2);
  if(muteOn(MUTE_MAIN) || muteOn(MUTE_SQUELCH))
  {
    spr.setTextColor(TH.box_off_text, TH.box_off_bg);
    sprintf(text, muteOn(MUTE_MAIN) ? "Muted" : "%d/sq", radioState.vol);
    spr.drawString(text, 48+x, 64+y+(0*16), 2);
    spr.setTextColor(TH.box_text);
  }
  else
  {
    spr.setTextColor(TH.box_text);
    spr.drawNumber(radioState.vol, 48+x, 64+y+(0*16), 2);
  }

  // Draw RDS PI code, if present
  uint16_t piCode = getRdsPiCode();
  if(piCode && radioState.mode == FM)
  {
    sprintf(text, "%04X", piCode);
    spr.drawString("PI:", 6+x, 64+y + (1*16), 2);
    spr.drawString(text, 48+x, 64+y + (1*16), 2);
  }
  else
  {
    spr.drawString("AVC:", 6+x, 64+y + (1*16), 2);

    if(radioState.mode==FM)
      sprintf(text, "n/a");
    else if(isSSB())
      sprintf(text, "%2.2ddB", radioState.ssbAvcIdx);
    else
      sprintf(text, "%2.2ddB", radioState.amAvcIdx);

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
    case CMD_MENU:       drawMenu(x, y, sx);       break;
    case CMD_SETTINGS:   drawSettings(x, y, sx);   break;
    case CMD_MODE:       drawMode(x, y, sx);       break;
    case CMD_STEP:       drawStep(x, y, sx);       break;
    case CMD_SEEK:       drawSeek(x, y, sx);       break;
    case CMD_SCAN:       drawScan(x, y, sx);       break;
    case CMD_BAND:       drawBand(x, y, sx);       break;
    case CMD_BANDWIDTH:  drawBandwidth(x, y, sx);  break;
    case CMD_THEME:      drawTheme(x, y, sx);      break;
    case CMD_UI:         drawUILayout(x, y, sx);   break;
    case CMD_VOLUME:     drawVolume(x, y, sx);     break;
    case CMD_AGC:        drawAgc(x, y, sx);        break;
    case CMD_SOFTMUTE:   drawSoftMuteMaxAtt(x, y, sx);break;
    case CMD_CAL:        drawCal(x, y, sx);        break;
    case CMD_AVC:        drawAvc(x, y, sx);        break;
    case CMD_FM_REGION:  drawFmRegion(x, y, sx);   break;
    case CMD_BRT:        drawBrt(x, y, sx);        break;
    case CMD_RDS:        drawRDSMode(x, y, sx);    break;
    case CMD_MEMORY:     drawMemory(x, y, sx);     break;
    case CMD_SLEEP:      drawSleep(x, y, sx);      break;
    case CMD_SLEEPMODE:  drawSleepMode(x, y, sx);  break;
    case CMD_USBMODE:    drawUSBMode(x, y, sx);    break;
    case CMD_BLEMODE:    drawBleMode(x, y, sx);    break;
    case CMD_WIFIMODE:   drawWiFiMode(x, y, sx);   break;
    case CMD_ZOOM:       drawZoom(x, y, sx);       break;
    case CMD_SCROLL:     drawScrollDir(x, y, sx);  break;
    case CMD_UTCOFFSET:  drawUTCOffset(x, y, sx);  break;
    case CMD_SQUELCH:    drawSquelch(x, y, sx);    break;
    default:             drawInfo(x, y, sx);       break;
  }
}
