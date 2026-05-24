#include "DisplayController.h"
#include "Common.h"
#include "Draw.h"     // for spr and tft

static bool displayAwake = true;

void displayInit(void) {
  // Nothing to initialize yet, but reserved for future display setup
}

void displaySleep(void) {
  ledcWrite(PIN_LCD_BL, 0);
  spr.fillSprite(TFT_BLACK);
  spr.pushSprite(0, 0);
  tft.writecommand(ST7789_DISPOFF);
  tft.writecommand(ST7789_SLPIN);
  displayAwake = false;
}

void displayWake(void) {
  tft.writecommand(ST7789_SLPOUT);
  delay(120);
  tft.writecommand(ST7789_DISPON);
  displayAwake = true;
}

void displaySetBrightness(uint16_t level) {
  ledcWrite(PIN_LCD_BL, level);
}

bool displayIsAwake(void) {
  return displayAwake;
}
