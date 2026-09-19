#include "Common.h"
#include "Draw.h"
#include "Menu.h"
#include "Ota.h"

#include <atomic>
#include <Update.h>
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
static const OtaMetadata otaMetadata __attribute__((section(".rodata_custom_desc"), used, aligned(4))) =
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

// Accumulate only the image prefix, even when the transport splits its headers.
static constexpr size_t OTA_PREFIX_SIZE = OTA_METADATA_OFFSET + sizeof(OtaMetadata);

struct OtaState
{
  std::atomic<bool> busy{false};
  std::atomic<OtaPhase> phase{OTA_IDLE};
  std::atomic<size_t> received{0};
  std::atomic<size_t> imageSize{0};
  std::atomic<const char *> error{nullptr};
  std::atomic<bool> resultPending{false};
  std::atomic<bool> cancelRequested{false};

  uint8_t prefix[OTA_PREFIX_SIZE];
};

static OtaState ota;

static bool otaFail(const char *error)
{
  Update.abort();
  ota.error = error;
  ota.phase = OTA_FAILED;
  ota.resultPending = true;
  return false;
}

static void otaPrepare(size_t imageSize)
{
  ota.error = nullptr;
  ota.resultPending = false;
  ota.received = 0;
  ota.imageSize = imageSize;
  ota.phase = OTA_WRITING;
}

bool otaBegin(size_t imageSize)
{
  if(ota.busy.exchange(true)) return false;
  ota.cancelRequested = false;
  otaPrepare(imageSize);
  return true;
}

static bool otaCheckMetadata()
{
  OtaMetadata metadata;
  memcpy(&metadata, ota.prefix + OTA_METADATA_OFFSET, sizeof(metadata));

  if(metadata.magic != OTA_METADATA_MAGIC || metadata.format != OTA_METADATA_FORMAT)
    return otaFail("Missing OTA metadata; use USB.");

  // A volatile read retains the embedded descriptor through linker GC/LTO.
  const volatile OtaMetadata &installed = otaMetadata;
  if(metadata.generation != installed.generation)
    return otaFail("Incompatible firmware; use USB.");
  if(metadata.variant != installed.variant)
    return otaFail("Wrong firmware variant; select the matching image.");
  return true;
}

bool otaWrite(uint8_t *data, size_t size)
{
  if(ota.phase.load() != OTA_WRITING) return false;
  if(ota.cancelRequested.load()) return otaFail("Update canceled.");
  size_t offset = ota.received.load();
  if(size > ota.imageSize.load() - offset)
    return otaFail("Firmware exceeds the declared size.");
  size_t chunkSize = size;
  if(offset < OTA_PREFIX_SIZE)
  {
    size_t count = OTA_PREFIX_SIZE - offset;
    if(count > size) count = size;
    memcpy(ota.prefix + offset, data, count);
    data += count;
    size -= count;
    if(offset + count < OTA_PREFIX_SIZE)
    {
      ota.received += chunkSize;
      return true;
    }
    if(!otaCheckMetadata()) return false;
    // Do not touch flash until compatibility has been checked.
    if(!Update.begin(ota.imageSize.load(), U_FLASH))
      return otaFail("Unable to start update.");
    if(Update.write(ota.prefix, OTA_PREFIX_SIZE) != OTA_PREFIX_SIZE)
      return otaFail(Update.errorString());
  }
  if(size && Update.write(data, size) != size)
    return otaFail(Update.errorString());
  ota.received += chunkSize;
  return true;
}

bool otaFinish()
{
  if(ota.cancelRequested.load()) return otaFail("Update canceled.");
  if(ota.phase.load() != OTA_WRITING || ota.received.load() < OTA_PREFIX_SIZE)
    return otaFail("Firmware is incomplete.");
  if(!Update.end()) return otaFail(Update.errorString());
  ota.phase = OTA_COMPLETE;
  return true;
}

void otaEndUpload()
{
  if(ota.phase.load() == OTA_COMPLETE || ota.phase.load() == OTA_REBOOT_PENDING)
    ota.phase = OTA_REBOOT_PENDING;
  else
  {
    if(ota.phase.load() == OTA_WRITING)
      otaFail(ota.cancelRequested.load()? "Update canceled." : "Upload interrupted.");
    ota.busy = false;
  }
}

OtaStatus otaStatus()
{
  OtaPhase phase = ota.phase.load();
  switch(phase)
  {
    case OTA_COMPLETE:
    case OTA_REBOOT_PENDING: return {phase, "DONE! Rebooting..."};
    case OTA_FAILED:
    {
      const char *error = ota.error.load();
      return {phase, String("Failed: ") + (error? error : "Update failed.")};
    }
    case OTA_WRITING:
    {
      size_t bytes = ota.received.load(), total = ota.imageSize.load();
      size_t percent = bytes * 100 / total;
      char text[48];
      snprintf(text, sizeof(text), "... %u bytes, %u%% ...",
               unsigned(bytes), unsigned(percent < 100? percent : 99));
      return {phase, text};
    }
    default: return {phase, ""};
  }
}

static void otaDrawProgress()
{
  OtaStatus status = otaStatus();
  drawScreen("Updating Firmware", status.message.c_str());
}

void otaTick()
{
  // Display drawing runs on the main task.
  // Browser uploads and page requests run on the async network task.
  OtaPhase phase = ota.phase.load();
  if(phase == OTA_WRITING || phase == OTA_COMPLETE || phase == OTA_REBOOT_PENDING)
  {
    currentCmd = CMD_NONE;
    uint32_t lastDraw = 0;
    bool firstUploadTick = true;
    while((phase = ota.phase.load()) == OTA_WRITING || phase == OTA_COMPLETE || phase == OTA_REBOOT_PENDING)
    {
      // Upload callbacks own Update and the prefix buffer; only signal from here.
      if(phase == OTA_WRITING)
      {
        if(firstUploadTick)
        {
          // Encoder movement before the upload must not cancel it.
          consumeAbortPending();
          firstUploadTick = false;
        }
        else if(consumeAbortPending()) ota.cancelRequested = true;
      }
      if(millis() - lastDraw >= 250 || phase == OTA_REBOOT_PENDING)
      {
        otaDrawProgress();
        lastDraw = millis();
      }
      if(phase == OTA_REBOOT_PENDING)
      {
        delay(1000);
        ESP.restart();
      }
      delay(20);
    }
  }
  // Show each result once on the receiver, for up to two seconds.
  // Keep the result available to the web page after returning to the radio screen.
  if(ota.resultPending.exchange(false))
  {
    currentCmd = CMD_NONE;
    otaDrawProgress();
    uint32_t shownAt = millis();
    while(millis() - shownAt < 2000)
    {
      // Stop waiting if another OTA operation has started.
      phase = ota.phase.load();
      if(phase != OTA_FAILED) break;
      delay(20);
    }
    drawScreen();
  }
}
