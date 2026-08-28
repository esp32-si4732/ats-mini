#include "Common.h"
#include "Splash.h"

#include <LittleFS.h>
#include <PNGdec.h>
#include <new>

static fs::File splashPngFile;

static void *splashPngOpen(const char *filename, int32_t *size)
{
  splashPngFile = LittleFS.open(filename, "r");
  if(!splashPngFile) return(nullptr);

  *size = splashPngFile.size();
  return(&splashPngFile);
}

static void splashPngClose(void *handle)
{
  fs::File *file = static_cast<fs::File *>(handle);
  if(file) file->close();
}

static int32_t splashPngRead(PNGFILE *pngFile, uint8_t *buffer, int32_t length)
{
  fs::File *file = static_cast<fs::File *>(pngFile->fHandle);
  return(file? file->read(buffer, length) : 0);
}

static int32_t splashPngSeek(PNGFILE *pngFile, int32_t position)
{
  fs::File *file = static_cast<fs::File *>(pngFile->fHandle);
  return(file? file->seek(position) : 0);
}

struct SplashDrawContext
{
  PNG *png;
  uint16_t *pixels;
};

static int splashPngDrawLine(PNGDRAW *draw)
{
  SplashDrawContext *context = static_cast<SplashDrawContext *>(draw->pUser);

  context->png->getLineAsRGB565(
    draw,
    context->pixels,
    PNG_RGB565_LITTLE_ENDIAN,
    0xffffffff
  );
  spr.pushImage(0, draw->y, draw->iWidth, 1, context->pixels);
  return(1);
}

bool splashDraw()
{
  if(!LittleFS.exists(SPLASH_PATH)) return(false);

  void *pngMemory = ps_malloc(sizeof(PNG));
  uint16_t *pixels = static_cast<uint16_t *>(ps_malloc(spr.width() * sizeof(uint16_t)));
  if(!pngMemory || !pixels)
  {
    free(pngMemory);
    free(pixels);
    return(false);
  }

  PNG *png = new (pngMemory) PNG;
  SplashDrawContext context = { png, pixels };

  int result = png->open(
    SPLASH_PATH,
    splashPngOpen,
    splashPngClose,
    splashPngRead,
    splashPngSeek,
    splashPngDrawLine
  );

  if(result == PNG_SUCCESS &&
     png->getWidth() == spr.width() && png->getHeight() == spr.height())
    result = png->decode(&context, PNG_FAST_PALETTE);
  else
    result = PNG_INVALID_FILE;

  png->close();
  png->~PNG();
  free(pngMemory);
  free(pixels);

  if(result != PNG_SUCCESS) return(false);

  spr.pushSprite(0, 0);
  return(true);
}

String splashValidate()
{
  void *memory = ps_malloc(sizeof(PNG));
  if(!memory)
    return("Not enough PSRAM to validate the PNG image.");

  PNG *png = new (memory) PNG;
  String error;

  int result = png->open(
    SPLASH_TEMP_PATH,
    splashPngOpen,
    splashPngClose,
    splashPngRead,
    splashPngSeek,
    [](PNGDRAW *) { return(1); }
  );

  if(result != PNG_SUCCESS)
    error = "The uploaded file is not a valid or supported PNG image.";
  else if((png->getWidth() != spr.width()) || (png->getHeight() != spr.height()))
    error = "The splash image must have a resolution of " +
            String(spr.width()) + "x" + String(spr.height()) + " pixels.";
  else
  {
    result = png->decode(nullptr, PNG_CHECK_CRC);

    if(result != PNG_SUCCESS)
      error = "The uploaded PNG image could not be decoded.";
  }

  png->close();
  png->~PNG();
  free(memory);

  return(error);
}
