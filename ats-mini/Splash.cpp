#include <LittleFS.h>

#include "Common.h"
#include "Splash.h"

static bool decodePng(LGFX_Sprite &target, const char *path)
{
  bool decoded = target.drawPngFile(LittleFS, path);
  target.releasePngMemory();
  return(decoded);
}

static bool splashHasExpectedDimensions(const char *path)
{
  fs::File file = LittleFS.open(path, "r");
  uint32_t dimensions[2];
  return(file && file.seek(16) &&
         file.read(reinterpret_cast<uint8_t *>(dimensions), sizeof(dimensions)) == sizeof(dimensions) &&
         __builtin_bswap32(dimensions[0]) == static_cast<uint32_t>(spr.width()) &&
         __builtin_bswap32(dimensions[1]) == static_cast<uint32_t>(spr.height()));
}

bool splashDraw()
{
  spr.fillSprite(TFT_BLACK);
  if(!decodePng(spr, SPLASH_PATH)) return(false);

  spr.pushSprite(0, 0);
  return(true);
}

String splashValidate()
{
  // Decode into a one-pixel DRAM sprite because uploads run in the network task.
  LGFX_Sprite validator(&tft);
  validator.setColorDepth(16);
  if(!validator.createSprite(1, 1))
    return("Not enough memory to validate the PNG image.");

  if(!decodePng(validator, SPLASH_TEMP_PATH) ||
     !splashHasExpectedDimensions(SPLASH_TEMP_PATH))
    return("The splash image must be a valid " + String(spr.width()) + "x" +
           String(spr.height()) + " PNG image.");

  return("");
}
