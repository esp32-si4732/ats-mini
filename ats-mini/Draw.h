#ifndef DRAW_H
#define DRAW_H

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
#define SAVE_OFFSET_X   90    // Preferences save icon horizontal offset
#define SAVE_OFFSET_Y    0    // Preferences save icon vertical offset
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
#define BLE_OFFSET_X   104    // BLE x offset
#define BLE_OFFSET_Y     0    // BLE y offset

void drawMessage(const char *msg);
void drawZoomedMenu(const char *text, bool force = false);
void drawScanGraphs(uint32_t freq);
void drawScreen(const char *statusLine1 = 0, const char *statusLine2 = 0);

void drawWiFiIndicator(int x, int y);
void drawSaveIndicator(int x, int y);
void drawBleIndicator(int x, int y);
void drawBandAndMode(const char *band, const char *mode, int x, int y);
void drawFrequency(uint32_t freq, int x, int y, int ux, int uy, uint8_t hl);
void drawLongStationName(const char *name, int x, int y);
void drawStationName(const char *name, int x, int y);
void drawSMeter(int strength, int x, int y);
void drawStereoIndicator(int x, int y, bool stereo = true);
bool drawWiFiStatus(const char *statusLine1, const char *statusLine2, int x, int y);
void drawRadioText(int y, int ymax);
void drawScale(uint32_t freq);

void drawLayoutDefault(const char *statusLine1, const char *statusLine2);
void drawLayoutSmeter(const char *statusLine1, const char *statusLine2);
void drawLayoutWaterfall(const char *statusLine1, const char *statusLine2);

void drawAbout();
void drawAboutHelp(uint8_t arrow);
void drawPropagation();
void drawUtility();

#endif /* DRAW_H */
