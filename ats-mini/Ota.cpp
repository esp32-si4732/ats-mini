#include "Common.h"
#include "Draw.h"
#include "Menu.h"
#include "Ota.h"
#include "Utils.h"

#include <atomic>
#include <memory>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Update.h>
#include <WiFi.h>
#include <esp_app_desc.h>
#include <esp_app_format.h>

#define OTA_HTTP_TIMEOUT_SECONDS 15

enum OtaVariant : uint8_t { OTA_OSPI = 1, OTA_QSPI = 2, OTA_LILYGO = 3 };
static const char *const otaVariantNames[] = {"", "ospi", "qspi", "lilygo-t-embed"};

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

// Use the CA bundle shipped with the ESP32 core, including after redirects.
extern const uint8_t caBundleStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t caBundleEnd[] asm("_binary_x509_crt_bundle_end");

struct OtaState
{
  std::atomic<bool> busy{false};
  std::atomic<OtaPhase> phase{OTA_IDLE};
  std::atomic<size_t> received{0};
  std::atomic<size_t> imageSize{0};
  std::atomic<uint16_t> latestVersion{0};
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
    case OTA_CHECK_QUEUED: return {phase, "Checking for updates..."};
    case OTA_AVAILABLE:
    {
      unsigned version = ota.latestVersion.load();
      char text[40];
      snprintf(text, sizeof(text), "New release v%u.%02u available.", version / 100, version % 100);
      return {phase, text};
    }
    case OTA_QUEUED:     return {phase, "Update requested..."};
    case OTA_CONNECTING: return {phase, "Connecting to GitHub..."};
    case OTA_COMPLETE:
    case OTA_REBOOT_PENDING: return {phase, "DONE! Rebooting..."};
    case OTA_CURRENT:    return {phase, "Already up to date."};
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
  statusShow("Updating Firmware", status.message.c_str(), 0);
  drawScreen();
}

bool otaRequestLatest(bool install)
{
  if(ota.busy.exchange(true)) return false;
  ota.cancelRequested = false;
  ota.error = nullptr;
  ota.resultPending = false;
  ota.phase = install? OTA_QUEUED : OTA_CHECK_QUEUED;
  return true;
}

static bool otaDownloadLatest(bool install)
{
  ota.phase = OTA_CONNECTING;
  otaDrawProgress();
  if(WiFi.status() != WL_CONNECTED)
    return otaFail("Connect Wi-Fi first.");
  if(!clockGetDate(nullptr, nullptr, nullptr, nullptr))
    return otaFail("Set the clock before updating.");

  NetworkClientSecure client;
  client.setCACertBundle(caBundleStart, caBundleEnd - caBundleStart);
  client.setHandshakeTimeout(OTA_HTTP_TIMEOUT_SECONDS);
  HTTPClient http;
  http.setConnectTimeout(OTA_HTTP_TIMEOUT_SECONDS * 1000);
  http.setTimeout(OTA_HTTP_TIMEOUT_SECONDS * 1000);
  http.useHTTP10(true);
  http.setUserAgent("ATS-Mini/" + String(getVersion(true)));

  // The latest-release redirect provides the tag without downloading JSON/HTML.
  if(!http.begin(client, FIRMWARE_URL "/releases/latest"))
    return otaFail("Unable to connect to GitHub.");
  int code = http.sendRequest("HEAD");
  String location = http.getLocation();
  http.end();

  if(consumeAbortPending()) return otaFail("Update canceled.");

  const String tagPrefix = FIRMWARE_URL "/releases/tag/";
  if((code != 302 && code != 301) || !location.startsWith(tagPrefix))
    return otaFail("Unable to find the latest release.");

  String tag = location.substring(tagPrefix.length());
  unsigned major, minor;
  char suffix;
  // A third conversion detects trailing characters, including prerelease suffixes.
  if(tag.length() > 7 || sscanf(tag.c_str(), "v%u.%u%c", &major, &minor, &suffix) != 2 ||
     major > UINT16_MAX / 100 || minor > 99 || major * 100 + minor > UINT16_MAX)
    return otaFail("Unexpected release version.");
  unsigned version = major * 100 + minor;
  ota.latestVersion = version;
  const bool newer = version > VER_APP;
  if(!newer || !install)
  {
    ota.phase = newer? OTA_AVAILABLE : OTA_CURRENT;
    ota.resultPending = true;
    return true;
  }
  String url = String(FIRMWARE_URL) + "/releases/download/" + tag +
               "/ats-mini-" + tag + "-" + otaVariantNames[otaMetadata.variant] + "-ota.bin";

  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if(!http.begin(client, url)) return otaFail("Unable to start firmware download.");
  code = http.GET();
  if(consumeAbortPending()) return otaFail("Update canceled.");
  if(code != HTTP_CODE_OK)
    return otaFail(code == 404? "Release has no matching OTA file." : "Firmware download failed.");
  int total = http.getSize();
  if(total < int(OTA_PREFIX_SIZE)) return otaFail("Invalid firmware download size.");
  otaPrepare(total);

  NetworkClient *stream = http.getStreamPtr();
  constexpr size_t bufferSize = 1024;
  std::unique_ptr<uint8_t, decltype(&free)> buffer(
    static_cast<uint8_t *>(ps_malloc(bufferSize)), &free);
  if(!buffer) return otaFail("Not enough PSRAM for update.");

  uint32_t lastData = millis(), lastDraw = 0;
  while(ota.received.load() < size_t(total))
  {
    if(consumeAbortPending()) return otaFail("Update canceled.");
    int available = stream->available();
    if(available > 0)
    {
      size_t count = size_t(available);
      if(count > bufferSize) count = bufferSize;
      size_t remaining = total - ota.received.load();
      if(count > remaining) count = remaining;
      int received = stream->read(buffer.get(), count);
      if(received > 0)
      {
        if(!otaWrite(buffer.get(), received)) return false;
        lastData = millis();
      }
    }
    else if(!http.connected())
      return otaFail("Firmware download interrupted.");
    if(millis() - lastData > OTA_HTTP_TIMEOUT_SECONDS * 1000)
      return otaFail("Firmware download timed out.");
    if(millis() - lastDraw >= 250)
    {
      otaDrawProgress();
      lastDraw = millis();
    }
    delay(1);
  }
  http.end();

  if(consumeAbortPending()) return otaFail("Update canceled.");
  if(!otaFinish()) return false;
  ota.phase = OTA_REBOOT_PENDING;
  return true;
}

void otaTick()
{
  // Display drawing and GitHub downloads run on the main task.
  // Browser uploads and page requests run on the async network task.
  OtaPhase phase = ota.phase.load();
  if(phase == OTA_CHECK_QUEUED || phase == OTA_QUEUED || phase == OTA_WRITING ||
     phase == OTA_COMPLETE || phase == OTA_REBOOT_PENDING)
  {
    currentCmd = CMD_NONE;
    if(phase == OTA_CHECK_QUEUED || phase == OTA_QUEUED)
    {
      // Clear stale cancellation input before starting the operation.
      consumeAbortPending();
      otaDownloadLatest(phase == OTA_QUEUED);
      if(ota.phase.load() != OTA_REBOOT_PENDING) ota.busy = false;
    }
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
    OtaStatus status = otaStatus();
    statusShow("Updating Firmware", status.message.c_str());
  }
}
