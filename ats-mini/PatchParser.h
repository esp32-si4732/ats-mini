#ifndef PATCH_PARSER_H
#define PATCH_PARSER_H

#include <stdint.h>
#include <stddef.h>

static constexpr size_t PATCH_MAX_BYTES = 32 * 1024;

// File: 24..32768 bytes, divisible by 8. Each block starts with
// [0x15, 0, LEN_BE16, metadata(4)]; LEN counts payload bytes.
// Follow with ceil(LEN/7) rows of [0x16, payload(7)]; unused bytes must be zero.
// A LEN=0 header is mandatory and must be the final row. Metadata is unchecked.
struct PatchFraming
{
  size_t size = 0;
  uint16_t remaining = 0;
  bool terminal = false;

  bool row(const uint8_t *data)
  {
    if(terminal || size + 8 > PATCH_MAX_BYTES) return false;
    size += 8;
    if(!remaining)
    {
      if(data[0] != 0x15 || data[1] != 0) return false;
      remaining = (uint16_t(data[2]) << 8) | data[3];
      terminal = remaining == 0;
      return size + ((remaining + 6U) / 7U) * 8U + (terminal? 0 : 8) <= PATCH_MAX_BYTES;
    }
    if(data[0] != 0x16) return false;
    uint8_t used = remaining < 7? remaining : 7;
    for(uint8_t i = used + 1; i < 8; i++)
      if(data[i]) return false;
    remaining -= used;
    return true;
  }

  bool finish() const { return terminal && !remaining && size >= 24; }
};

// Accumulate only a partial row across arbitrary upload chunk boundaries.
struct PatchStream
{
  PatchFraming framing;
  uint8_t data[8] = {};
  uint8_t used = 0;

  bool write(const uint8_t *bytes, size_t length)
  {
    if(length > PATCH_MAX_BYTES - framing.size - used) return false;
    for(size_t i = 0; i < length; i++)
    {
      data[used++] = bytes[i];
      if(used == 8)
      {
        used = 0;
        if(!framing.row(data)) return false;
      }
    }
    return true;
  }

  bool finish() const { return !used && framing.finish(); }
};

#endif
