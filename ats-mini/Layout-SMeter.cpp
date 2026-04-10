#include "Common.h"
#include "Themes.h"
#include "Menu.h"
#include "Draw.h"

static int getInterpolatedStrength(int rssi)
{
  const int am_thresholds[] = {1, 2, 3, 4, 10, 16, 22, 28, 34, 44, 54, 64, 74, 84, 94, 95, 96};
  const int am_values[]     = {1, 4, 7, 10, 13, 16, 19, 22, 25, 28, 31, 34, 37, 40, 43, 46, 49};
  const int fm_thresholds[] = {1, 2, 8, 14, 24, 34, 44, 54, 64, 74, 76, 77};
  const int fm_values[]     = {1, 19, 22, 25, 28, 31, 34, 37, 40, 43, 46, 49};
  int num_thresholds;
  const int *thresholds;
  const int *values;

  if(currentMode!=FM)
  {
    num_thresholds = ITEM_COUNT(am_thresholds);
    thresholds = am_thresholds;
    values = am_values;
  }
  else
  {
    num_thresholds = ITEM_COUNT(fm_thresholds);
    thresholds = fm_thresholds;
    values = fm_values;
  }

  for(int i = 0; i < num_thresholds; i++)
  {
    if(rssi <= thresholds[i])
    {
      if(!i) return values[i];
      int interval = thresholds[i] - thresholds[i-1];
      if(!interval) return values[i];
      float position = (float)(rssi - thresholds[i-1]) / interval;
      float interpolated = values[i-1] + position * (values[i] - values[i-1]);
      return (int)(interpolated + 0.5);
    }
  }

  return values[num_thresholds - 1];
}

//
// Draw small tuner scale
//
static void drawSmallScale(uint32_t freq, int y)
{
  const Band *band = getCurrentBand();
  const uint16_t scaleStart = 51;
  const uint16_t scaleEnd = 269;

  for(int i=scaleStart+3; i<=scaleEnd-3; i+=2) spr.drawPixel(i, y, TH.scale_line);
  spr.drawCircle(scaleStart, y, 3, TH.scale_line);
  spr.drawCircle(scaleEnd, y, 3, TH.scale_line);
  spr.fillCircle(scaleStart + (scaleEnd-scaleStart) * (freq - band->minimumFreq) / (band->maximumFreq - band->minimumFreq), y, 3, TH.scale_pointer);

  char lim[8];
  spr.setTextColor(TH.scale_text);
  spr.setTextDatum(MC_DATUM);
  if(band->bandType==FM_BAND_TYPE)
    sprintf(lim, "%0.2f", band->minimumFreq/100.00);
  else
    sprintf(lim, "%u", band->minimumFreq);
  spr.drawString(lim, scaleStart-27, y, 2);
  if(band->bandType==FM_BAND_TYPE)
    sprintf(lim, "%0.2f", band->maximumFreq/100.00);
  else
    sprintf(lim, "%u", band->maximumFreq);
  spr.drawString(lim, scaleEnd+27, y, 2);
}

//
// Draw alternative stereo indicator
//
static void drawAltStereoIndicator(int x, int y, bool stereo = true)
{
  if(stereo)
  {
    spr.drawCircle(x - 4, y, 7, TH.stereo_icon);
    spr.drawCircle(x + 4, y, 7, TH.stereo_icon);
  }
  // Add an "else" statement here to draw a mono indicator
}

static void drawLargeSMeter(int rssi, int strength, int x, int y)
{
  // S-Meter legend
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TH.scale_text);

  int last_x = x;
  for(int i=0; i<16; i++)
  {
    if(i%2)
    {
      int text_width = 0;
      int text_x = x + (i * 15) - 13;
      if(i < 10)  text_width = spr.drawNumber(i, text_x, 20+y, 2);
      if(i == 11) text_width = spr.drawString("+20", text_x, 20+y, 2);
      if(i == 13) text_width = spr.drawString("+40", text_x, 20+y, 2);
      if(i == 15) text_width = spr.drawString("+60", text_x, 20+y, 2);

      // Draw the dotted line from end of previous number to start of current number
      for(int px=last_x; px<text_x-text_width/2; px++) {
        if(px & 1) spr.drawPixel(px, 28+y, TH.scale_line);
      }
      last_x = text_x + text_width/2;
    }
  }

  // Draw the remaining dotted line after the last number
  for(int px=last_x; px <= x+15*16+2; px++) {
    if(px & 1) spr.drawPixel(px, 28+y, TH.scale_line);
  }

  spr.setTextDatum(BL_DATUM);
  spr.drawString("S", x - 10, 36 + y, 2);
  spr.setTextDatum(BR_DATUM);
  spr.drawNumber(rssi, x - 15, 40 + y, 4);

  // S-Meter
  for(int i=0; i<49; i++)
    if (i<28 && i<strength)
      spr.fillRect(x+(i*5), 11+y, 3, 10, TH.smeter_bar);
    else if (i<strength)
      spr.fillRect(x+(i*5), 11+y, 3, 10, TH.smeter_bar_plus);
    else
      spr.fillRect(x+(i*5), 11+y, 3, 10, TH.smeter_bar_empty);
}

static void drawLargeSNMeter(int snr, int x, int y)
{
  spr.setTextColor(TH.scale_text);
  spr.setTextDatum(BL_DATUM);
  spr.drawString("N", x - 10, 12 + y, 2);
  spr.setTextDatum(BR_DATUM);
  spr.drawNumber(snr, x - 15, 16 + y, 4);

  // SN-Meter
  int snrbars = snr * 45 / 128.0;
  for(int i=0; i<49; i++)
    if (i<snrbars)
      spr.fillRect(x+(i*5), y - 1, 3, 10, TH.smeter_bar);
    else
      spr.fillRect(x+(i*5), y - 1, 3, 10, TH.smeter_bar_empty);
}

//
// Draw alternative screen layout with the large S-meter.
//
void drawLayoutSmeter(const char *statusLine1, const char *statusLine2)
{
  // Draw preferences write request icon
  drawSaveIndicator(SAVE_OFFSET_X, SAVE_OFFSET_Y);

  // Draw BLE icon
  //drawBleIndicator(BLE_OFFSET_X, BLE_OFFSET_Y);

  // Draw battery indicator & voltage
  bool has_voltage = drawBattery(BATT_OFFSET_X, BATT_OFFSET_Y);

  // Draw WiFi icon
  drawWiFiIndicator(has_voltage ? WIFI_OFFSET_X : BATT_OFFSET_X - 13, WIFI_OFFSET_Y);

  // Set font we are going to use
  spr.setFreeFont(&Orbitron_Light_24);

  // Draw band and mode
  drawBandAndMode(
    getCurrentBand()->bandName,
    bandModeDesc[currentMode],
    BAND_OFFSET_X, BAND_OFFSET_Y
  );

  if(switchThemeEditor())
  {
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(TH.text_warn);
    spr.drawString(TH.name, 319, BATT_OFFSET_Y + 17, 2);
  }

  // Draw frequency, units, and optionally highlight a digit
  drawFrequency(
    currentFrequency,
    FREQ_OFFSET_X, FREQ_OFFSET_Y,
    FUNIT_OFFSET_X, FUNIT_OFFSET_Y,
    currentCmd == CMD_FREQ ? getFreqInputPos() + (pushAndRotate ? 0x80 : 0) : 100
  );

  // Show station or channel name, if present
  if(*getStationName() == 0xFF)
    drawLongStationName(getStationName() + 1, MENU_OFFSET_X + 1 + 76 + MENU_DELTA_X + 2, RDS_OFFSET_Y);
  else if(*getStationName())
    drawStationName(getStationName(), RDS_OFFSET_X, RDS_OFFSET_Y);

  // Draw band scale
  drawSmallScale(isSSB()? (currentFrequency + currentBFO/1000) : currentFrequency, 120);

  // Draw left-side menu/info bar
  // @@@ FIXME: Frequency display (above) intersects the side bar!
  drawSideBar(currentCmd, ALT_MENU_OFFSET_X, ALT_MENU_OFFSET_Y, MENU_DELTA_X);

  // Indicate FM pilot detection (stereo indicator)
  drawAltStereoIndicator(ALT_STEREO_OFFSET_X, ALT_STEREO_OFFSET_Y, (currentMode==FM) && rx.getCurrentPilot());

  if(currentCmd == CMD_SCAN)
  {
    drawScanGraphs(isSSB()? (currentFrequency + currentBFO/1000) : currentFrequency);
  }
  else if(!drawWiFiStatus(statusLine1, statusLine2, STATUS_OFFSET_X, STATUS_OFFSET_Y))
  {
    // Show radio text if present, else show S & SN meters
    if(*getRadioText() || *getProgramInfo())
      drawRadioText(STATUS_OFFSET_Y, STATUS_OFFSET_Y + 25);
    else
    {
      // Draw SN-meter
      drawLargeSNMeter(snr, ALT_METER_OFFSET_X, ALT_METER_OFFSET_Y);
      // Draw S-meter
      drawLargeSMeter(rssi, getInterpolatedStrength(rssi), ALT_METER_OFFSET_X, ALT_METER_OFFSET_Y);
    }
  }
}
