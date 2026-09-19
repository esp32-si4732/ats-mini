#include "Common.h"

#include <esp_app_desc.h>
#include <esp_app_format.h>

enum OtaVariant : uint8_t { OTA_OSPI = 1, OTA_QSPI = 2, OTA_LILYGO = 3 };

struct OtaMetadata
{
  uint32_t magic;
  uint16_t version;
  uint8_t format;
  uint8_t generation;
  uint8_t variant;
  uint8_t reserved[3];
};

static constexpr uint32_t OTA_METADATA_MAGIC = 0x4D535441; // "ATSM" in the image
static constexpr uint8_t OTA_METADATA_FORMAT = 1;
// Retain the descriptor before the OTA reader is introduced.
static const OtaMetadata otaMetadata __attribute__((section(".rodata_custom_desc"), used, retain, aligned(4))) =
{
  OTA_METADATA_MAGIC, VER_APP, OTA_METADATA_FORMAT, VER_OTA,
#if defined(LILYGO_SI473X)
  OTA_LILYGO,
#elif defined(CONFIG_SPIRAM_MODE_OCT)
  OTA_OSPI,
#else
  OTA_QSPI,
#endif
  {0}
};
static constexpr size_t OTA_METADATA_OFFSET = sizeof(esp_image_header_t) +
  sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t);
static_assert(sizeof(OtaMetadata) == 12, "OTA metadata must have a stable layout");
static_assert(OTA_METADATA_OFFSET == 288, "Update the release metadata checker for this image format");
static_assert(VER_OTA > 0 && VER_OTA <= UINT8_MAX, "VER_OTA must fit in the metadata");

static_assert(VER_APP > 0 && VER_APP <= UINT16_MAX, "VER_APP must fit in the metadata");
