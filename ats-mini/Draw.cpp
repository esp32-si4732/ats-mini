#include <qrcode.h>
#include "Common.h"
#include "Themes.h"
#include "Storage.h"
#include "Utils.h"
#include "Menu.h"
#include <LittleFS.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <map> // Used for storing Spectrum Scan RSSI values across screen redraws. 

// Display position control
#define MENU_OFFSET_X    0    // Menu horizontal offset
#define MENU_OFFSET_Y   18    // Menu vertical offset
#define ALT_MENU_OFFSET_X    0    // Menu horizontal offset
#define ALT_MENU_OFFSET_Y    0    // Menu vertical offset
#define MENU_DELTA_X    10    // Menu width delta
#define METER_OFFSET_X   0    // Meter horizontal offset
#define METER_OFFSET_Y   0    // Meter vertical offset
#define ALT_METER_OFFSET_X  75    // Meter horizontal offset
#define ALT_METER_OFFSET_Y 136    // Meter vertical offset
#define SAVE_OFFSET_X   90    // EEPROM save icon horizontal offset
#define SAVE_OFFSET_Y    0    // EEPROM save icon vertical offset
#define FREQ_OFFSET_X  250    // Frequency horizontal offset
#define FREQ_OFFSET_Y   62    // Frequency vertical offset
#define FUNIT_OFFSET_X 255    // Frequency Unit horizontal offset
#define FUNIT_OFFSET_Y  45    // Frequency Unit vertical offset
#define BAND_OFFSET_X  150    // Band horizontal offset
#define BAND_OFFSET_Y    9    // Band vertical offset
#define ALT_STEREO_OFFSET_X 232
#define ALT_STEREO_OFFSET_Y 24
#define RDS_OFFSET_X   165    // RDS horizontal offset
#define RDS_OFFSET_Y    94    // RDS vertical offset
#define STATUS_OFFSET_X 160   // Status & RDS text horizontal offset
#define STATUS_OFFSET_Y 135   // Status & RDS text vertical offset
#define BATT_OFFSET_X  288    // Battery meter x offset
#define BATT_OFFSET_Y    0    // Battery meter y offset
#define WIFI_OFFSET_X  237    // WiFi x offset
#define WIFI_OFFSET_Y    0    // WiFi y offset

static std::map<int, int> spectrumBars; // Used for storing Spectrum Scan RSSI values across screen redraws. 

static void displayQRCode(esp_qrcode_handle_t qrcode) {
    int size = esp_qrcode_get_size(qrcode);
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            if (esp_qrcode_get_module(qrcode, x, y)) {
                spr.fillRect(2 + x * 4, 170 - 2 - size * 4 + y * 4, 4, 4, TH.text);
            }
        }
    }
}

static void drawAboutCommon(uint8_t arrow)
{
  if(arrow & 3) spr.fillRect(282, 11, 22, 3, TH.text_muted);
  if(arrow & 2) spr.fillTriangle(279, 12, 285, 8, 285, 16, TH.text_muted);
  if(arrow & 1) spr.fillTriangle(307, 12, 301, 8, 301, 16, TH.text_muted);

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TH.text_muted, TH.bg);
  spr.drawString(RECEIVER_NAME, 0, 0, 4);
  spr.setTextColor(TH.text, TH.bg);
  spr.drawString(getVersion(), 2, 25, 2);
}

//
// Show HELP screen
//
void drawAboutHelp(uint8_t arrow)
{
  drawAboutCommon(arrow);
  esp_qrcode_config_t qrcode_config = ESP_QRCODE_CONFIG_DEFAULT();
  qrcode_config.display_func = displayQRCode;
  esp_qrcode_generate(&qrcode_config, MANUAL_URL);
  spr.drawString("Scan the QR code to read", 130, 70 + 16 * -1, 2);
  spr.drawString("the User Manual.", 130, 70 + 16 * 0, 2);
  spr.drawString("Click the encoder button", 130, 70 + 16 * 1, 2);
  spr.drawString("to continue.", 130, 70 + 16 * 2, 2);
  if(arrow)
  {
    spr.drawString("Rotate the encoder to see", 130, 70 + 16 * 3, 2);
    spr.drawString("the next page.", 130, 70 + 16 * 4, 2);
  }
  else
  {
    spr.drawString("To see this screen again,", 130, 70 + 16 * 4, 2);
    spr.drawString("go to Menu->Settings->About.", 130, 70 + 16 * 5, 2);
  }
  spr.pushSprite(0, 0);
}

//
// Show SYSTEM screen
//
static void drawAboutSystem(uint8_t arrow)
{
  drawAboutCommon(arrow);

  char text[100];
  sprintf(
    text,
    "CPU: %s r%i, %lu MHz",
    ESP.getChipModel(),
    ESP.getChipRevision(),
    ESP.getCpuFreqMHz()
  );
  spr.drawString(text, 2, 70 + 16 * -1, 2);

  sprintf(
    text,
    "FLASH: %luM, %luk (%luk), FS %luk (%luk)",
    ESP.getFlashChipSize() / (1024U * 1024U),
    ESP.getFreeSketchSpace() / 1024U,
    (ESP.getFreeSketchSpace() - ESP.getSketchSize()) / 1024U,
    (unsigned long)LittleFS.totalBytes() / 1024U,
    (unsigned long)(LittleFS.totalBytes() - LittleFS.usedBytes()) / 1024U
  );
  spr.drawString(text, 2, 70 + 16 * 0, 2);

  nvs_stats_t nvs_stats;
  nvs_get_stats(NULL, &nvs_stats);
  sprintf(
    text,
    "NVS: TOTAL %u, USED %u, FREE %u",
    nvs_stats.total_entries,
    nvs_stats.used_entries,
    nvs_stats.free_entries
  );
  spr.drawString(text, 2, 70 + 16 * 1, 2);

  sprintf(
    text,
    "MEM: HEAP %luk (%luk), PSRAM %luk (%luk)",
    ESP.getHeapSize()/1024U, ESP.getFreeHeap()/1024U,
    ESP.getPsramSize()/1024U, ESP.getFreePsram()/1024U
  );
  spr.drawString(text, 2, 70 + 16 * 2, 2);

  sprintf(
    text,
    "Display ID: %08lX, STAT: %02X%08lX",
    tft.readcommand32(ST7789_RDDID, 1),
    tft.readcommand8(ST7789_RDDST, 1),
    tft.readcommand32(ST7789_RDDST, 2)
  );
  spr.drawString(text, 2, 70 + 16 * 3, 2);

  char *ip = getWiFiIPAddress();
  sprintf(text, "WiFi MAC: %s%s%s", getMACAddress(), *ip ? ", IP: " : "", *ip ? ip : "");
  spr.drawString(text, 2, 70 + 16 * 4, 2);

  for(int i=0 ; i<8 ; i++)
  {
    uint16_t rgb = (i&1? 0x001F:0) | (i&2? 0x07E0:0) | (i&4? 0xF800:0);
    spr.fillRect(i*40, 160, 40, 20, rgb);
  }
  spr.pushSprite(0, 0);
}

//
// Show AUTHORS screen
//
static void drawAboutAuthors(uint8_t arrow)
{
  drawAboutCommon(arrow);
  spr.drawString(FIRMWARE_URL, 2, 25 + 16, 2);
  spr.drawString(AUTHORS_LINE1, 2, 70, 2);
  spr.drawString(AUTHORS_LINE2, 2, 70 + 16, 2);
  spr.drawString(AUTHORS_LINE3, 2, 70 + 16 * 2, 2);
  spr.drawString(AUTHORS_LINE4, 2, 70 + 16 * 3, 2);
  spr.pushSprite(0, 0);
}

//
// Draw ABOUT screens
//
static void drawAbout()
{
  switch(doAbout(0)) {
  case 0: drawAboutHelp(1); break;
  case 1: drawAboutAuthors(3); break;
  case 2: drawAboutSystem(2); break;
  default: break;
  }
}

// Draw zoomed menu item
void drawZoomedMenu(const char *text)
{
  if (!zoomMenu) return;

  spr.fillSmoothRoundRect(RDS_OFFSET_X - 70 + 1, RDS_OFFSET_Y - 3 + 1, 148, 26, 4, TH.menu_bg);
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TH.menu_item, TH.menu_bg);
  spr.drawString(text, RDS_OFFSET_X + 5, RDS_OFFSET_Y, 4);
  spr.drawSmoothRoundRect(RDS_OFFSET_X - 70, RDS_OFFSET_Y - 3, 4, 4, 150, 28, TH.menu_border, TH.menu_bg);
}

//
// Show "Loading SSB" message
//
void drawLoadingSSB()
{
  if(sleepOn()) return;

  spr.setTextDatum(MC_DATUM);
  spr.fillSmoothRoundRect(80, 40, 160, 40, 4, TH.text);
  spr.fillSmoothRoundRect(81, 41, 158, 38, 4, TH.menu_bg);
  spr.setTextColor(TH.text, TH.menu_bg);
  spr.drawString("Loading SSB", 160, 62, 4);
  spr.pushSprite(0, 0);
}

//
// Draw band and mode indicators
//
static void drawBandAndMode(const char *band, const char *mode, int x, int y)
{
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TH.band_text, TH.bg);
  uint16_t band_width = spr.drawString(band, x, y);

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TH.mode_text, TH.bg);
  uint16_t mode_width = spr.drawString(mode, x + band_width / 2 + 12, y + 8, 2);

  spr.drawSmoothRoundRect(x + band_width / 2 + 7, y + 7, 4, 4, mode_width + 8, 17, TH.mode_border, TH.bg);
}

//
// Draw radio text
//
static void drawRadioText(int y, int ymax)
{
  const char *rt = getRadioText();

  // Draw potentially multi-line radio text
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TH.rds_text, TH.bg);
  for(; *rt && (y<ymax) ; y+=17, rt+=strlen(rt)+1)
    spr.drawString(rt, 160, y, 2);

  // Show program info if we have it and there is enough space
  if((y<ymax) && *getProgramInfo())
    spr.drawString(getProgramInfo(), 160, y, 2);
}

//
// Draw frequency
//
static void drawFrequency(uint32_t freq, int x, int y, int ux, int uy, uint8_t hl)
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
  spr.setTextColor(TH.freq_text, TH.bg);

  if(currentMode==FM)
  {
    // Determine where underscore is located
    li = hl<ITEM_COUNT(hlDigitsFM)? &hlDigitsFM[hl] : 0;

    // FM frequency
    spr.drawFloat(freq/100.00, 2, x, y, 7);
    spr.setTextDatum(ML_DATUM);
    spr.setTextColor(TH.funit_text, TH.bg);
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
    spr.setTextColor(TH.funit_text, TH.bg);
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
static void drawScale(uint32_t freq)
{
  spr.fillTriangle(156, 120, 160, 130, 164, 120, TH.scale_pointer);
  spr.drawLine(160, 130, 160, 169, TH.scale_pointer);

  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TH.scale_text, TH.bg);

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
      uint16_t lineColor =
        i == 20 && (offset == 0 || ((freq % 5) == 0 && offset == 1)) ?
          TH.scale_pointer : TH.scale_line;
      if(spectrumScan && (spectrumState == 0)) // If Scpectrum Scan/Signal Survey option is enabled AND the state flag is "requested".
      {
        rx.setFrequency(freq * 10);
        rx.getCurrentReceivedSignalQuality(); // For more precision adjust MAX_DELAY_AFTER_SET_FREQUENCY on SI4735.h to at least 60ms (according to SI47xx Programming Guide, tuning takes at least 60ms com complete after issuing the command!)

        int rssi = rx.getCurrentRSSI();
        int slevel = getStrength(rssi);

        spectrumBars[freq] = rssi;

        if (slevel < 7) lineColor = 0xF800;
        else if (slevel < 10) lineColor = 0xffe0;
        else lineColor = 0x07e0;

        spr.drawLine(x, 150, x, 150 - rssi, lineColor);
      }
      else if(spectrumScan)
      {
        spr.drawLine(x, 150, x, 150 - spectrumBars[freq], lineColor);
      }
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
  
  if(spectrumScan && spectrumState == 0) // Return to original freq after we did the survey scan and indicate the scan has been done
  {
   rx.setFrequency(currentFrequency);
   spectrumState = 1; // Scan flag in "done" state.
  }

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
  spr.setTextColor(TH.scale_text, TH.bg);
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
// Draw S-meter
//
static void drawSMeter(int strength, int x, int y)
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

static void drawLargeSMeter(int rssi, int strength, int x, int y)
{
  // S-Meter legend
  for(int i=x; i<=x+15*16 + 2; i+=2) spr.drawPixel(i, 28+y, TH.scale_line);
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TH.scale_text, TH.bg);

  for(int i=0; i<16; i++) {
    if (i%2) {
      if (i < 10) spr.drawNumber(i, x+(i*15)-13, 20+y, 2);
      if (i == 11) spr.drawString("+20", x+(i*15)-13, 20+y, 2);
      if (i == 13) spr.drawString("+40", x+(i*15)-13, 20+y, 2);
      if (i == 15) spr.drawString("+60", x+(i*15)-13, 20+y, 2);
    }
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
  spr.setTextColor(TH.scale_text, TH.bg);
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
// Draw stereo indicator
//
static void drawStereoIndicator(int x, int y, bool stereo = true)
{
  if(stereo)
  {
    // Split S-meter into two rows
    spr.fillRect(15 + x, 7 + y, 4 * 17 - 2, 2, TH.bg);
  }
  // Add an "else" statement here to draw a mono indicator
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

//
// Draw RDS station name (also CB channel, etc)
//
static void drawStationName(const char *name, int x, int y)
{
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TH.rds_text, TH.bg);
  spr.drawString(name, x, y, 4);
}

//
// Draw long (EIBI) station name
//
static void drawLongStationName(const char *name, int x, int y)
{
  int width = spr.textWidth(name, 2);
  spr.setTextColor(TH.rds_text, TH.bg);

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
// Draw network status
//
static bool drawWiFiStatus(const char *statusLine1, const char *statusLine2, int x, int y)
{
  if(statusLine1 || statusLine2)
  {
    // Draw two lines of network status
    spr.setTextDatum(TC_DATUM);
    spr.setTextColor(TH.rds_text, TH.bg);
    if(statusLine1) spr.drawString(statusLine1, x, y, 2);
    if(statusLine2) spr.drawString(statusLine2, x, y+17, 2);
    return(true);
  }
  return(false);
}

static void drawLayoutSmeter(const char *statusLine1, const char *statusLine2)
{
  // Draw EEPROM write request icon
  drawEepromIndicator(SAVE_OFFSET_X, SAVE_OFFSET_Y);

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
    spr.setTextColor(TH.text_warn, TH.bg);
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

  if(!drawWiFiStatus(statusLine1, statusLine2, STATUS_OFFSET_X, STATUS_OFFSET_Y))
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

void drawLayoutDefault(const char *statusLine1, const char *statusLine2)
{
  // Draw EEPROM write request icon
  drawEepromIndicator(SAVE_OFFSET_X, SAVE_OFFSET_Y);

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
    spr.setTextColor(TH.text_warn, TH.bg);
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

  // Draw left-side menu/info bar
  // @@@ FIXME: Frequency display (above) intersects the side bar!
  drawSideBar(currentCmd, MENU_OFFSET_X, MENU_OFFSET_Y, MENU_DELTA_X);

  // Draw S-meter
  drawSMeter(getStrength(rssi), METER_OFFSET_X, METER_OFFSET_Y);

  // Indicate FM pilot detection (stereo indicator)
  drawStereoIndicator(METER_OFFSET_X, METER_OFFSET_Y, (currentMode==FM) && rx.getCurrentPilot());

  if(!drawWiFiStatus(statusLine1, statusLine2, STATUS_OFFSET_X, STATUS_OFFSET_Y))
  {
    // Show radio text if present, else show frequency scale
    if(*getRadioText() || *getProgramInfo())
      drawRadioText(STATUS_OFFSET_Y, STATUS_OFFSET_Y + 25);
    else
      drawScale(isSSB()? (currentFrequency + currentBFO/1000) : currentFrequency);
  }
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

#ifdef ENABLE_HOLDOFF
  // Update if not tuning
  if(!tuning_flag)
  {
    spr.pushSprite(0, 0);
  }
#else
  // No hold off
  spr.pushSprite(0, 0);
#endif
}
