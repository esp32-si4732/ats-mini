#include "Common.h"
#include "Themes.h"
#include "Storage.h"
#include "Utils.h"
#include "Menu.h"
#include "Ble.h"
#include "Draw.h"

//
// Draw preferences write indicator
//
void drawSaveIndicator(int x, int y)
{
  if(prefsAreWritten() || switchThemeEditor())
  {
    // Draw preferences write request icon
    spr.fillRect(x+3, y+2, 3, 5, TH.save_icon);
    spr.fillTriangle(x+1, y+7, x+7, y+7, x+4, y+10, TH.save_icon);
    spr.drawLine(x, y+12, x, y+13, TH.save_icon);
    spr.drawLine(x, y+13, x+8, y+13, TH.save_icon);
    spr.drawLine(x+8, y+13, x+8, y+12, TH.save_icon);
  }
}

//
// Draw Bluetooth indicator
//
void drawBleIndicator(int x, int y)
{
  int8_t status = getBleStatus();

  // If need to draw BLE icon...
  if(status || switchThemeEditor())
  {
    uint16_t color = (status>0) ? TH.rf_icon_conn : TH.rf_icon;

    // For the editor, alternate between BLE states every ~8 seconds
    if(switchThemeEditor())
      color = millis()&0x2000? TH.rf_icon_conn : TH.rf_icon;

    spr.drawLine(x+3, y+1, x+3, y+13, color);
    spr.drawLine(x+3, y+1, x+6, y+4, color);
    spr.drawLine(x+6, y+4, x, y+10, color);
    spr.drawLine(x, y+4, x+6, y+10, color);
    spr.drawLine(x+6, y+10, x+3, y+13, color);
  }
}

//
// Draw WiFi indicator
//
void drawWiFiIndicator(int x, int y)
{
  int8_t status = getWiFiStatus();

  // If need to draw WiFi icon...
  if(status || switchThemeEditor())
  {
    uint16_t color = (status>0) ? TH.rf_icon_conn : TH.rf_icon;

    // For the editor, alternate between WiFi states every ~8 seconds
    if(switchThemeEditor())
      color = millis()&0x2000? TH.rf_icon_conn : TH.rf_icon;

    spr.drawSmoothArc(x, 15+y, 14, 13, 150, 210, color, TH.bg);
    spr.drawSmoothArc(x, 15+y, 9, 8, 150, 210, color, TH.bg);
    spr.drawSmoothArc(x, 15+y, 4, 3, 150, 210, color, TH.bg);
  }
}

//
// Draw network status
//
bool drawWiFiStatus(const char *statusLine1, const char *statusLine2, int x, int y)
{
  if(statusLine1 || statusLine2)
  {
    // Draw two lines of network status
    spr.setTextDatum(TC_DATUM);
    spr.setTextColor(TH.rds_text);
    if(statusLine1) spr.drawString(statusLine1, x, y, 2);
    if(statusLine2) spr.drawString(statusLine2, x, y+17, 2);
    return(true);
  }

  return(false);
}

//
// Draw zoomed menu item
//
void drawZoomedMenu(const char *text, bool force)
{
  if (!zoomMenu && !force) return;

  spr.fillSmoothRoundRect(RDS_OFFSET_X - 72 + 1, RDS_OFFSET_Y - 3 + 1, 152, 26, 4, TH.menu_bg);
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TH.menu_item);
  spr.drawString(text, RDS_OFFSET_X + 5, RDS_OFFSET_Y, 4);
  spr.drawSmoothRoundRect(RDS_OFFSET_X - 72, RDS_OFFSET_Y - 3, 4, 4, 154, 28, TH.menu_border, TH.menu_bg);
}

//
// Show overlay message in large letters
//
void drawMessage(const char *msg)
{
  if(sleepOn()) return;

  drawZoomedMenu(msg, true);
  spr.pushSprite(0, 0);
}

//
// Draw band and mode indicators
//
void drawBandAndMode(const char *band, const char *mode, int x, int y)
{
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TH.band_text);
  uint16_t band_width = spr.drawString(band, x, y);

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TH.mode_text);
  uint16_t mode_width = spr.drawString(mode, x + band_width / 2 + 12, y + 8, 2);

  spr.drawSmoothRoundRect(x + band_width / 2 + 7, y + 7, 4, 4, mode_width + 8, 17, TH.mode_border, TH.bg);
}

//
// Draw radio text
//
void drawRadioText(int y, int ymax)
{
  const char *rt = getRadioText();

  // Draw potentially multi-line radio text
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TH.rds_text);
  for(; *rt && (y<ymax) ; y+=17, rt+=strlen(rt)+1)
    spr.drawString(rt, 160, y, 2);

  // Show program info if we have it and there is enough space
  if((y<ymax) && *getProgramInfo())
    spr.drawString(getProgramInfo(), 160, y, 2);
}

//
// Draw frequency
//
void drawFrequency(uint32_t freq, int x, int y, int ux, int uy, uint8_t hl)
{
  struct Line { int x, y, w; };

  const Line hlDigitsFM[] =
  {
    { x - 30 - 32 * 0 -  0, y + 28, 27 }, //         .01
    { x - 30 - 32 * 0 - 16, y + 28, 27 + 16 }, //    .05
    { x - 30 - 32 * 1 -  0, y + 28, 27 }, //         .10
    { x - 30 - 32 * 1 - 22, y + 28, 27 + 22 }, //    .50
    { x - 30 - 32 * 2 - 12, y + 28, 27 }, //        1.00
    { x - 30 - 32 * 2 - 28, y + 28, 27 + 16 }, //   5.00
    { x - 30 - 32 * 3 - 12, y + 28, 27 }, //       10.00
    { x - 30 - 32 * 3 - 28, y + 28, 27 + 16 }, //  50.00
    { x - 30 - 32 * 4 +  4, y + 28, 11 }, //      100.00
  };

  const Line hlDigitsAMSSB[] =
  {
    { x + 12 + 14 * 2 -  0, y + 28, 12 }, //           .001
    { x + 12 + 14 * 2 -  7, y + 28, 12 + 7 }, //       .005
    { x + 12 + 14 * 1 -  0, y + 28, 12 }, //           .010
    { x + 12 + 14 * 1 -  7, y + 28, 12 + 7 }, //       .050
    { x + 12 + 14 * 0 -  0, y + 28, 12 }, //           .100
    { x + 12 + 14 * 0 - 11, y + 28, 12 + 11 }, //      .500
    { x - 30 - 32 * 0 -  0, y + 28, 27 }, //          1.000
    { x - 30 - 32 * 0 - 16, y + 28, 27 + 16 }, //     5.000
    { x - 30 - 32 * 1 -  0, y + 28, 27 }, //         10.000
    { x - 30 - 32 * 1 - 16, y + 28, 27 + 16 }, //    50.000
    { x - 30 - 32 * 2 -  0, y + 28, 27 }, //        100.000
    { x - 30 - 32 * 2 - 16, y + 28, 27 + 16 }, //   500.000
    { x - 30 - 32 * 3 -  0, y + 28, 27 }, //       1000.000
    { x - 30 - 32 * 3 - 16, y + 28, 27 + 16 }, //  5000.000
    { x - 30 - 32 * 4 -  0, y + 28, 27 }, //      10000.000
  };

  // Top bit specifies if the digit selector is on
  bool selectOn = hl & 0x80;
  const struct Line *li;

  // Lower 7 bits specify the selected digit
  hl &= 0x7F;

  spr.setTextDatum(MR_DATUM);
  spr.setTextColor(TH.freq_text);

  if(currentMode==FM)
  {
    // Determine where underscore is located
    li = hl<ITEM_COUNT(hlDigitsFM)? &hlDigitsFM[hl] : 0;

    // FM frequency
    spr.drawFloat(freq/100.00, 2, x, y, 7);
    spr.setTextDatum(ML_DATUM);
    spr.setTextColor(TH.funit_text);
    spr.drawString("MHz", ux, uy);
  }
  else
  {
    // Determine where underscore is located
    li = hl<ITEM_COUNT(hlDigitsAMSSB)? &hlDigitsAMSSB[hl] : 0;

    if(isSSB())
    {
      // SSB frequency
      char text[32];
      freq = freq * 1000 + currentBFO;
      sprintf(text, "%3.3lu", freq / 1000);
      spr.drawString(text, x, y, 7);
      spr.setTextDatum(ML_DATUM);
      sprintf(text, ".%3.3lu", freq % 1000);
      spr.drawString(text, 4+x, 17+y, 4);
    }
    else
    {
      // AM frequency
      spr.drawNumber(freq, x, y, 7);
      spr.setTextDatum(ML_DATUM);
      spr.drawString(".000", 4+x, 17+y, 4);
    }

    // SSB/AM frequencies are measured in kHz
    spr.setTextColor(TH.funit_text);
    spr.drawString("kHz", ux, uy);
  }

  // If drawing an underscore...
  if(li)
  {
    if(selectOn)
    {
      spr.fillRoundRect(li->x + 1, li->y - 1, li->w - 2, 3, 1, TH.freq_hl_sel);
      spr.fillTriangle(li->x, li->y, li->x + 2, li->y - 2, li->x + 2, li->y + 2, TH.freq_hl_sel);
      spr.fillTriangle(li->x + li->w - 1, li->y, li->x + li->w - 3, li->y - 2, li->x + li->w - 3, li->y + 2, TH.freq_hl_sel);
    }
    else
    {
      spr.fillRoundRect(li->x, li->y - 1, li->w, 3, 1, TH.freq_hl);
    }
  }
}

//
// Draw tuner scale
//
void drawScale(uint32_t freq)
{
  // Scale pointer
  spr.fillTriangle(156, 120, 160, 130, 164, 120, TH.scale_pointer);
  spr.drawLine(160, 130, 160, 169, TH.scale_pointer);

  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TH.scale_text);

  // Extra frequencies to draw outside the screen boundaries
  // (ensures frequency numbers don't disappear at the edges)
  int16_t slack = 3;

  // Scale offset
  int16_t offset = ((freq % 10) / 10.0 + slack) * 8;

  // Start drawing frequencies from the left
  freq = freq / 10 - 20 - slack;

  // Get band edges
  const Band *band = getCurrentBand();
  uint32_t minFreq = band->minimumFreq / 10;
  uint32_t maxFreq = band->maximumFreq / 10;

  for(int i=0 ; i<(slack + 41 + slack) ; i++, freq++)
  {
    int16_t x = i * 8 - offset;
    if(freq >= minFreq && freq <= maxFreq)
    {
      uint16_t lineColor = (i==20) && (!offset || (!(freq%5) && offset==1))?
        TH.scale_pointer : TH.scale_line;

      if((freq % 10) == 0)
      {
        spr.drawLine(x, 169, x, 150, lineColor);
        spr.drawLine(x + 1, 169, x + 1, 150, lineColor);
        if(currentMode == FM)
          spr.drawFloat(freq / 10.0, 1, x, 140, 2);
        else if(freq >= 100)
          spr.drawFloat(freq / 100.0, 3, x, 140, 2);
        else
          spr.drawNumber(freq * 10, x, 140, 2);
      }
      else if((freq % 5) == 0 && (freq % 10) != 0)
      {
        spr.drawLine(x, 169, x, 155, lineColor);
        spr.drawLine(x + 1, 169, x + 1, 155, lineColor);
      }
      else
      {
        spr.drawLine(x, 169, x, 160, lineColor);
      }
    }
  }
}

//
// Draw S-meter
//
void drawSMeter(int strength, int x, int y)
{
  spr.drawTriangle(x + 1, y + 1, x + 11, y + 1, x + 6, y + 6, TH.smeter_icon);
  spr.drawLine(x + 6, y + 1, x + 6, y + 14, TH.smeter_icon);

  for(int i=0 ; i<strength ; i++)
  {
    if(i<10)
      spr.fillRect(15+x + (i*4), 2+y, 2, 12, TH.smeter_bar);
    else
      spr.fillRect(15+x + (i*4), 2+y, 2, 12, TH.smeter_bar_plus);
  }
}

//
// Draw stereo indicator
//
void drawStereoIndicator(int x, int y, bool stereo)
{
  if(stereo)
  {
    // Split S-meter into two rows
    spr.fillRect(15 + x, 7 + y, 4 * 17 - 2, 2, TH.bg);
  }
  // Add an "else" statement here to draw a mono indicator
}

//
// Draw RDS station name (also CB channel, etc)
//
void drawStationName(const char *name, int x, int y)
{
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TH.rds_text);
  spr.drawString(name, x, y, 4);
}

//
// Draw long (EIBI) station name
//
void drawLongStationName(const char *name, int x, int y)
{
  int width = spr.textWidth(name, 2);
  spr.setTextColor(TH.rds_text);

  if((x + width) >= 320)
  {
    spr.setTextDatum(TL_DATUM);
    spr.drawString(name, x, y, 2);
  }
  else if(width <= 60)
  {
    spr.setTextDatum(TC_DATUM);
    spr.drawString(name, x + (320 - x) / 3, y, 2);
  }
  else
  {
    spr.setTextDatum(TC_DATUM);
    spr.drawString(name, x + (320 - x + width) / 4, y, 2);
  }
}

//
// Draw scan graphs
//
void drawScanGraphs(uint32_t freq)
{
  // Scale offset
  int16_t offset = (freq % 10) / 10.0 * 8;

  // Start drawing frequencies from the left
  freq = freq / 10 - 20;

  // Get band edges
  const Band *band = getCurrentBand();
  uint32_t minFreq = band->minimumFreq / 10;
  uint32_t maxFreq = band->maximumFreq / 10;

  for(int i=0 ; i<41 ; i++, freq++)
  {
    int16_t x = i * 8 - offset;

    if(freq >= minFreq && freq <= maxFreq)
    {
      if((freq % 5) == 0) {
        for(int y=0; y<42; y+=2) {
          spr.drawPixel(x, 169-y, TH.scan_grid);
        }
      }

      if((freq+1) <= maxFreq) {
        for(int xd=x; xd<(x+8); xd+=2) {
          spr.drawPixel(xd, 169-40, TH.scan_grid);
          spr.drawPixel(xd, 169-30, TH.scan_grid);
          spr.drawPixel(xd, 169-20, TH.scan_grid);
          spr.drawPixel(xd, 169-10, TH.scan_grid);
          spr.drawPixel(xd, 169-0, TH.scan_grid);
        }
        int snr1 = 40 * scanGetSNR(freq * 10);
        int snr2 = 40 * scanGetSNR((freq+1) * 10);
        spr.drawLine(x, 169-snr1, x+8, 169-snr2, TH.scan_snr);
        int rssi1 = 40 * scanGetRSSI(freq * 10);
        int rssi2 = 40 * scanGetRSSI((freq+1) * 10);
        spr.drawLine(x, 169-rssi1, x+8, 169-rssi2, TH.scan_rssi);
      }
    }
  }
  // Scale pointer
  spr.fillTriangle(156, 125, 160, 130, 164, 125, TH.scale_pointer);
  spr.drawLine(160, 130, 160, 169, TH.scale_pointer);
}

//
// Draw screen according to given command
//
void drawScreen(const char *statusLine1, const char *statusLine2)
{
  if(sleepOn()) return;

  // Clear screen buffer
  spr.fillSprite(TH.bg);

  // About screen is a special case
  if(currentCmd==CMD_ABOUT)
  {
    drawAbout();
    return;
  }

  switch(uiLayoutIdx)
  {
    case UI_SMETER:
      drawLayoutSmeter(statusLine1, statusLine2);
      break;
    default:
      drawLayoutDefault(statusLine1, statusLine2);
      break;
  }

  spr.pushSprite(0, 0);
}
