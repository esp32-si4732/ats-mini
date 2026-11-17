#include "Common.h"
#include "Screenshot.h"

static bool writeScreenshotChunk(Stream& stream, const uint8_t* data, size_t size)
{
  if(&stream != &Serial) return stream.write(data, size) == size;

  // HWCDC can discard queued bytes on a false disconnect. Only enqueue data
  // that fits; screenshot output has no competing Serial writer.
  uint32_t lastProgress = millis();
  while(size)
  {
    if(millis() - lastProgress >= stream.getTimeout()) return false;
    int space = Serial.availableForWrite();
    if(space <= 0)
    {
      yield();
      continue;
    }
    size_t chunk = size < (size_t)space ? size : (size_t)space;
    size_t written = Serial.write(data, chunk);
    if(!written) return false;
    lastProgress = millis();
    data += written;
    size -= written;
  }
  return true;
}

// BMP dimensions match the display sprite; all fields are little-endian.
static const uint8_t bmpHeader[] = {
  'B', 'M',
  (uint8_t)BMP_FILE_SIZE, (uint8_t)(BMP_FILE_SIZE >> 8),
  (uint8_t)(BMP_FILE_SIZE >> 16), (uint8_t)(BMP_FILE_SIZE >> 24),
  0, 0, 0, 0, 66, 0, 0, 0,
  40, 0, 0, 0,                         // BITMAPINFOHEADER size
  (uint8_t)DISPLAY_WIDTH, (uint8_t)(DISPLAY_WIDTH >> 8), 0, 0,
  (uint8_t)DISPLAY_HEIGHT, (uint8_t)(DISPLAY_HEIGHT >> 8), 0, 0,
  1, 0, 16, 0, 3, 0, 0, 0,            // One plane, RGB565 bitfields
  0, 0, 0, 0, 0, 0, 0, 0,             // Image size and X resolution
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Y resolution and palette
  0, 0xF8, 0, 0, 0xE0, 0x07, 0, 0, 0x1F, 0, 0, 0 // RGB masks
};

//
// Send the screen as a hex ('C') or binary ('c') RGB565 BMP.
//
template<typename Write>
static bool captureScreen(Write write, bool binary)
{
  const uint16_t *fb = (const uint16_t *)spr.getBuffer();
  uint8_t buf[256];
  if(!fb) return false;

  // Keep chunks full across the header and rows. All output is added in pairs.
  uint8_t *p = buf;
  auto appendPair = [&](uint8_t first, uint8_t second) {
    *p++ = first;
    *p++ = second;
    if(p == buf + sizeof(buf))
    {
      if(!write(buf, sizeof(buf))) return false;
      p = buf;
    }
    return true;
  };
  auto appendHexByte = [&](uint8_t byte) {
    static const char hex[] = "0123456789abcdef";
    return appendPair(hex[byte >> 4], hex[byte & 0xF]);
  };
  auto appendBmpPair = [&](uint8_t first, uint8_t second) {
    return binary ? appendPair(first, second)
                  : appendHexByte(first) && appendHexByte(second);
  };

  if(!binary && !appendPair('\r', '\n')) return false;
  for(size_t i=0 ; i<sizeof(bmpHeader) ; i+=2)
  {
    if(!appendBmpPair(bmpHeader[i], bmpHeader[i + 1])) return false;
  }
  if(!binary && !appendPair('\r', '\n')) return false;

  // Send rows bottom-up. The unrotated sprite stores contiguous rows of
  // byte-swapped RGB565 words; emit the high byte first for BMP pixel data.
  for(int y=DISPLAY_HEIGHT-1 ; y>=0 ; y--)
  {
    const uint16_t *row = fb + (uint32_t)y * DISPLAY_WIDTH;
    for(int x=0 ; x<DISPLAY_WIDTH ; x++)
    {
      uint16_t v = row[x];
      if(!appendBmpPair(v >> 8, v & 0xFF)) return false;
    }
    if(!binary && !appendPair('\r', '\n')) return false;
  }
  if(p != buf && !write(buf, p - buf)) return false;
  return true;
}

void screenshotWrite(Stream& stream, bool binary)
{
  if(!captureScreen([&](const uint8_t *data, size_t size) {
    return writeScreenshotChunk(stream, data, size);
  }, binary)) return;
  // HWCDC flush() can discard pending output on a false disconnect.
  if(&stream != &Serial) stream.flush();
}

bool screenshotCaptureBmp(uint8_t *bmp)
{
  return captureScreen([&](const uint8_t *data, size_t length) {
    memcpy(bmp, data, length);
    bmp += length;
    return true;
  }, true);
}
