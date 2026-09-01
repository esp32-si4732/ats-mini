#include "Common.h"
#include "Storage.h"
#include "Themes.h"
#include "Utils.h"
#include "Menu.h"
#include "Draw.h"
#include <LittleFS.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <qrcode.h>

static void displayQRCode(esp_qrcode_handle_t qrcode)
{
  int size = esp_qrcode_get_size(qrcode);

  for(int y = 0 ; y < size ; y++)
    for(int x = 0 ; x < size ; x++)
      if(esp_qrcode_get_module(qrcode, x, y))
        spr.fillRect(2 + x * 4, 170 - 2 - size * 4 + y * 4, 4, 4, TH.text);
}

static void drawAboutCommon(uint8_t arrow)
{
  if(arrow & 3) spr.fillRect(282, 11, 22, 3, TH.text_muted);
  if(arrow & 2) spr.fillTriangle(279, 12, 285, 8, 285, 16, TH.text_muted);
  if(arrow & 1) spr.fillTriangle(307, 12, 301, 8, 301, 16, TH.text_muted);

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TH.text_muted);
  spr.drawString(RECEIVER_DESC, 0, 0, FONT_LARGE);
  spr.setTextColor(TH.text);
  spr.drawString(getVersion(), 2, 25, FONT_SMALL);
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
  spr.drawString("Scan the QR code to read", 130, 70 + 16 * -1, FONT_SMALL);
  spr.drawString("the User Manual.", 130, 70 + 16 * 0, FONT_SMALL);
  spr.drawString("Click the encoder button", 130, 70 + 16 * 1, FONT_SMALL);
  spr.drawString("to continue.", 130, 70 + 16 * 2, FONT_SMALL);
  if(arrow)
  {
    spr.drawString("Rotate the encoder to see", 130, 70 + 16 * 3, FONT_SMALL);
    spr.drawString("the next page.", 130, 70 + 16 * 4, FONT_SMALL);
  }
  else
  {
    spr.drawString("To see this screen again,", 130, 70 + 16 * 4, FONT_SMALL);
    spr.drawString("go to Menu->Settings->About.", 130, 70 + 16 * 5, FONT_SMALL);
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
  spr.drawString(text, 2, 70 + 16 * -1, FONT_SMALL);

  sprintf(
    text,
    "FLASH: %luM, %luk (%luk), FS %luk (%luk)",
    ESP.getFlashChipSize() / (1024U * 1024U),
    ESP.getFreeSketchSpace() / 1024U,
    (ESP.getFreeSketchSpace() - ESP.getSketchSize()) / 1024U,
    (unsigned long)LittleFS.totalBytes() / 1024U,
    (unsigned long)(LittleFS.totalBytes() - LittleFS.usedBytes()) / 1024U
  );
  spr.drawString(text, 2, 70 + 16 * 0, FONT_SMALL);

  nvs_stats_t nvs_stats;
  nvs_get_stats(STORAGE_PARTITION, &nvs_stats);
  sprintf(
    text,
    "NVS: TOTAL %u, USED %u, FREE %u",
    nvs_stats.total_entries,
    nvs_stats.used_entries,
    nvs_stats.free_entries
  );
  spr.drawString(text, 2, 70 + 16 * 1, FONT_SMALL);

  sprintf(
    text,
    "MEM: HEAP %luk (%luk), PSRAM %luk (%luk)",
    ESP.getHeapSize()/1024U, ESP.getFreeHeap()/1024U,
    ESP.getPsramSize()/1024U, ESP.getFreePsram()/1024U
  );
  spr.drawString(text, 2, 70 + 16 * 2, FONT_SMALL);

  sprintf(
    text,
    "Display ID: %06lX",
    (unsigned long)tft.getDisplayId()
  );
  spr.drawString(text, 2, 70 + 16 * 3, FONT_SMALL);

  char *ip = getWiFiIPAddress();
  sprintf(text, "WiFi MAC: %s%s%s", getMACAddress(), *ip ? ", IP: " : "", *ip ? ip : "");
  spr.drawString(text, 2, 70 + 16 * 4, FONT_SMALL);

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
  spr.drawString(FIRMWARE_URL, 2, 25 + 16, FONT_SMALL);
  spr.drawString(AUTHORS_LINE1, 2, 70, FONT_SMALL);
  spr.drawString(AUTHORS_LINE2, 2, 70 + 16, FONT_SMALL);
  spr.drawString(AUTHORS_LINE3, 2, 70 + 16 * 2, FONT_SMALL);
  spr.drawString(AUTHORS_LINE4, 2, 70 + 16 * 3, FONT_SMALL);
  spr.pushSprite(0, 0);
}

//
// Draw ABOUT screens
//
void drawAbout()
{
  switch(doAbout(0))
  {
    case 0: drawAboutHelp(1); break;
    case 1: drawAboutAuthors(3); break;
    case 2: drawAboutSystem(2); break;
    default: break;
  }
}
