#include "Common.h"
#include "Themes.h"
#include "Menu.h"
#include "Draw.h"

// History buffers
#define HISTORY_LEN 320
static uint8_t rssiHistory[HISTORY_LEN];
static uint8_t snrHistory[HISTORY_LEN];
static uint16_t historyHead = 0;
static bool historyInit = false;

static void updateHistory()
{
  if (!historyInit) {
    memset(rssiHistory, 0, sizeof(rssiHistory));
    memset(snrHistory, 0, sizeof(snrHistory));
    historyInit = true;
  }
  
  rssiHistory[historyHead] = rssi;
  snrHistory[historyHead] = snr;
  historyHead = (historyHead + 1) % HISTORY_LEN;
}

//
// Draw Waterfall/History Layout
//
void drawLayoutWaterfall(const char *statusLine1, const char *statusLine2)
{
  // ---------------------------------------------------------
  // Top Section (Copied from Layout-SMeter mostly)
  // ---------------------------------------------------------
  
  // Draw preferences write request icon
  drawSaveIndicator(SAVE_OFFSET_X, SAVE_OFFSET_Y);

  // Draw BLE icon
  //drawBleIndicator(BLE_OFFSET_X, BLE_OFFSET_Y);

  // Draw battery indicator & voltage
  bool has_voltage = drawBattery(BATT_OFFSET_X, BATT_OFFSET_Y);

  // Draw WiFi icon
  drawWiFiIndicator(has_voltage ? WIFI_OFFSET_X : BATT_OFFSET_X - 13, WIFI_OFFSET_Y);

  // Set font
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

  // Draw frequency
  drawFrequency(
    currentFrequency,
    FREQ_OFFSET_X, FREQ_OFFSET_Y,
    FUNIT_OFFSET_X, FUNIT_OFFSET_Y,
    currentCmd == CMD_FREQ ? getFreqInputPos() + (pushAndRotate ? 0x80 : 0) : 100
  );

  // Show station or channel name
  if(*getStationName() == 0xFF)
    drawLongStationName(getStationName() + 1, MENU_OFFSET_X + 1 + 76 + MENU_DELTA_X + 2, RDS_OFFSET_Y);
  else if(*getStationName())
    drawStationName(getStationName(), RDS_OFFSET_X, RDS_OFFSET_Y);

  // Draw small scale (same as S-Meter layout)
  // We reuse the scale drawing logic from Layout-SMeter (which is static in that file, 
  // so we might need to copy it or make it public. 
  // Wait, drawSmallScale was static in Layout-SMeter.cpp. 
  // I cannot call it. I will skip it or copy it.
  // Copying it is safer to avoid modifying Draw.h too much.
  
  // Draw left-side menu/info bar
  drawSideBar(currentCmd, ALT_MENU_OFFSET_X, ALT_MENU_OFFSET_Y, MENU_DELTA_X);

  // ---------------------------------------------------------
  // Bottom Section: Signal History Graph
  // ---------------------------------------------------------

  updateHistory();

  int graphX = 80; // Start after the menu
  int graphY = 120;
  int graphW = 240; // 320 - 80
  int graphH = 50;

  // Draw Graph Box
  spr.drawRect(graphX, graphY, graphW, graphH, TH.box_border);
  spr.fillRect(graphX+1, graphY+1, graphW-2, graphH-2, TH.box_bg);

  // Draw Grid Lines (Horizontal)
  spr.drawLine(graphX, graphY + graphH/2, graphX + graphW, graphY + graphH/2, TH.box_border);
  
  // Draw Labels
  spr.setTextDatum(MR_DATUM);
  spr.setTextColor(TH.box_text);
  spr.drawString("S", graphX - 2, graphY + 10, 2);
  spr.drawString("N", graphX - 2, graphY + graphH - 10, 2);

  // Draw RSSI History (Top half of graph, or full graph?)
  // Let's draw RSSI as a filled area or line.
  
  int startIdx = (historyHead - graphW + HISTORY_LEN) % HISTORY_LEN;
  
  for (int i = 0; i < graphW - 1; i++) {
      int idx = (startIdx + i) % HISTORY_LEN;
      int idxNext = (startIdx + i + 1) % HISTORY_LEN;
      
      // RSSI (0-100 typical)
      int val1 = rssiHistory[idx];
      int val2 = rssiHistory[idxNext];
      
      // Scale to graph height
      int h1 = (val1 * (graphH - 2)) / 100; 
      int h2 = (val2 * (graphH - 2)) / 100;
      
      if(h1 > graphH-2) h1 = graphH-2;
      if(h2 > graphH-2) h2 = graphH-2;

      // Draw Line
      spr.drawLine(graphX + i + 1, graphY + graphH - 1 - h1, graphX + i + 2, graphY + graphH - 1 - h2, TH.smeter_bar);
      
      // Optional: Draw SNR as a different color line?
      // SNR (0-30 typical)
      int s1 = snrHistory[idx];
      int s2 = snrHistory[idxNext];
      int sh1 = (s1 * (graphH - 2)) / 40; // Scale 0-40 SNR to height
      int sh2 = (s2 * (graphH - 2)) / 40;
      
       if(sh1 > graphH-2) sh1 = graphH-2;
       if(sh2 > graphH-2) sh2 = graphH-2;
       
      spr.drawLine(graphX + i + 1, graphY + graphH - 1 - sh1, graphX + i + 2, graphY + graphH - 1 - sh2, TH.scan_snr);
  }

  // Draw current numeric values overlay
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(TH.text);
  spr.drawNumber(rssi, graphX + graphW - 2, graphY + 2, 2);
  spr.setTextDatum(BR_DATUM);
  spr.setTextColor(TH.scan_snr);
  spr.drawNumber(snr, graphX + graphW - 2, graphY + graphH - 2, 2);
}
