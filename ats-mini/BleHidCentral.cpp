#include <map>
#include <string>

// Arduino-ESP32's NimBLE wrapper leaks BLERemoteDescriptor objects when HID notify
// subscription goes through the normal public path:
//   registerForNotify() -> subscribe() -> setNotify() -> retrieveDescriptors()
// We keep the hack confined to this translation unit and use it only to preload
// the CCCD through the private filtered path.
#define private public
#include <BLERemoteCharacteristic.h>
#undef private

#include "BleHidCentral.h"
#include "Draw.h"
#include <string.h>

static BLEUUID hidServiceUUID((uint16_t)0x1812);
static BLEUUID deviceInfoServiceUUID((uint16_t)0x180A);
static BLEUUID reportCharUUID((uint16_t)0x2A4D);
static BLEUUID pnpIdUUID((uint16_t)0x2A50);
static BLEUUID cccdUUID((uint16_t)0x2902);

static constexpr uint16_t consumerUsagePlayPause = 0x00CD;
static constexpr uint16_t consumerUsageScanNextTrack = 0x00B5;
static constexpr uint16_t consumerUsageScanPreviousTrack = 0x00B6;
static constexpr uint16_t consumerUsageVolumeIncrement = 0x00E9;
static constexpr uint16_t consumerUsageVolumeDecrement = 0x00EA;

static constexpr uint16_t consumerBitfield16VolumeDecrement = 1u << 0;
static constexpr uint16_t consumerBitfield16VolumeIncrement = 1u << 1;
static constexpr uint16_t consumerBitfield16ScanPreviousTrack = 1u << 3;
static constexpr uint16_t consumerBitfield16ScanNextTrack = 1u << 4;
static constexpr uint16_t consumerBitfield16PlayPause = 1u << 5;

BleHidCentral* BleHidCentral::activeInstance = nullptr;

namespace {

static bool matchesDeviceInfoField(BLERemoteService* service, const BLEUUID& uuid, const uint8_t* expected, size_t expectedLength)
{
  if (service == nullptr || expected == nullptr) return false;

  BLERemoteCharacteristic* characteristic = service->getCharacteristic(uuid);
  if (characteristic == nullptr) return false;

  String value = characteristic->readValue();
  return value.length() == expectedLength && memcmp(value.c_str(), expected, expectedLength) == 0;
}

static BLERemoteDescriptor* getFilteredDescriptor(BLERemoteCharacteristic* characteristic, const BLEUUID& uuid)
{
  if (characteristic == nullptr) return nullptr;

  std::string descriptorKey = uuid.toString().c_str();
  auto found = characteristic->m_descriptorMap.find(descriptorKey);
  if (found != characteristic->m_descriptorMap.end())
    return found->second;

  characteristic->removeDescriptors();
  if (!characteristic->retrieveDescriptors(&uuid))
    return nullptr;

  found = characteristic->m_descriptorMap.find(descriptorKey);
  return found == characteristic->m_descriptorMap.end() ? nullptr : found->second;
}

static bool subscribeWithFilteredCccd(BLERemoteCharacteristic* characteristic, notify_callback callback)
{
  if (getFilteredDescriptor(characteristic, cccdUUID) == nullptr) return false;

  if (!characteristic->subscribe(true, callback, true)) return false;
  return characteristic->m_descriptorMap.find(cccdUUID.toString().c_str()) != characteristic->m_descriptorMap.end();
}

static BLERemoteCharacteristic* findReportCharacteristic(BLERemoteService* service, uint16_t handle)
{
  if (service == nullptr) return nullptr;

  std::map<uint16_t, BLERemoteCharacteristic*>* characteristics = nullptr;
  service->getCharacteristics(&characteristics);
  if (characteristics == nullptr) return nullptr;

  for (auto const& entry : *characteristics)
  {
    BLERemoteCharacteristic* characteristic = entry.second;
    if (characteristic->getHandle() == handle &&
        characteristic->getUUID().equals(reportCharUUID) &&
        characteristic->canNotify())
      return characteristic;
  }

  return nullptr;
}

static bool decodeConsumerBitfield16(
  const uint8_t* data,
  size_t length,
  bool& hasScanNext,
  bool& hasScanPrevious,
  bool& hasVolumeIncrement,
  bool& hasVolumeDecrement,
  bool& hasPlayPause)
{
  if (data == nullptr || length < 2) return false;

  uint16_t bits = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
  hasScanNext = (bits & consumerBitfield16ScanNextTrack) != 0;
  hasScanPrevious = (bits & consumerBitfield16ScanPreviousTrack) != 0;
  hasVolumeIncrement = (bits & consumerBitfield16VolumeIncrement) != 0;
  hasVolumeDecrement = (bits & consumerBitfield16VolumeDecrement) != 0;
  hasPlayPause = (bits & consumerBitfield16PlayPause) != 0;
  return true;
}

static bool decodeConsumerUsage16(
  const uint8_t* data,
  size_t length,
  bool& hasScanNext,
  bool& hasScanPrevious,
  bool& hasVolumeIncrement,
  bool& hasVolumeDecrement,
  bool& hasPlayPause)
{
  if (data == nullptr || length != 2) return false;

  uint16_t usage = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
  hasScanNext = usage == consumerUsageScanNextTrack;
  hasScanPrevious = usage == consumerUsageScanPreviousTrack;
  hasVolumeIncrement = usage == consumerUsageVolumeIncrement;
  hasVolumeDecrement = usage == consumerUsageVolumeDecrement;
  hasPlayPause = usage == consumerUsagePlayPause;
  return true;
}

}  // namespace

bool BleHidCentral::tryMatchMiniKeyboard(
  BLERemoteService* deviceInfoService,
  DecoderKind& decoder,
  uint16_t& reportHandle)
{
  // MINI_KEYBOARD:
  // - Device Information PnP ID  = 02 AC 05 2C 02 1B 01
  // - Consumer report value handle = 50
  static constexpr uint8_t pnpId[] = {0x02, 0xAC, 0x05, 0x2C, 0x02, 0x1B, 0x01};
  static constexpr uint16_t matchedReportHandle = 50;

  if (!matchesDeviceInfoField(deviceInfoService, pnpIdUUID, pnpId, sizeof(pnpId)))
    return false;

  decoder = DecoderKind::ConsumerUsage16;
  reportHandle = matchedReportHandle;
  return true;
}

bool BleHidCentral::tryMatchVol20(
  BLERemoteService* deviceInfoService,
  DecoderKind& decoder,
  uint16_t& reportHandle)
{
  // VOL20:
  // - Device Information PnP ID  = 01 D7 07 00 00 10 01
  // - Consumer report value handle = 56
  static constexpr uint8_t pnpId[] = {0x01, 0xD7, 0x07, 0x00, 0x00, 0x10, 0x01};
  static constexpr uint16_t matchedReportHandle = 56;

  if (!matchesDeviceInfoField(deviceInfoService, pnpIdUUID, pnpId, sizeof(pnpId)))
    return false;

  decoder = DecoderKind::ConsumerBitfield16;
  reportHandle = matchedReportHandle;
  return true;
}

BleHidState BleHidCentral::update()
{
  if (!pendingState.rotation && !pendingState.wasClicked && !pendingState.wasShortPressed &&
      playPauseClickPending && (int32_t)(millis() - playPauseClickDeadline) >= 0)
  {
    pendingState.wasClicked = true;
    playPauseClickPending = false;
    playPauseClickDeadline = 0;
  }

  pendingState.isPressed =
    scanNextPressed || scanPreviousPressed || (virtualPushUntil && (int32_t)(virtualPushUntil - millis()) > 0);
  BleHidState result = pendingState;
  pendingState = {};
  return result;
}

void BleHidCentral::configureSecurity()
{
  security.setCapability(ESP_IO_CAP_NONE);
  security.setAuthenticationMode(true, false, true);
  BLESecurity::setForceAuthentication(false);
  BLEDevice::setSecurityCallbacks(&securityCallbacks);
}

void BleHidCentral::configureScan(BLEScan& scan)
{
  scan.setInterval(BLE_SCAN_INTERVAL);
  scan.setWindow(BLE_SCAN_WINDOW);
  scan.setActiveScan(true);
}

void BleHidCentral::configureClient()
{
  BLEClient* currentClient = client();
  if (currentClient == nullptr) return;
  currentClient->setMTU(185);
}

void BleHidCentral::onScanStart()
{
  char statusLine[40];
  uint8_t maxAttempts = MAX_SCAN_ATTEMPTS;

  drawScreen();
  if (maxAttempts)
  {
    snprintf(statusLine, sizeof(statusLine), "Scanning for BLE HID... %u/%u", scanAttempts, maxAttempts);
    drawScreen(statusLine);
  }
  else
    drawScreen("Scanning for BLE HID...");

  delay(500);
}

bool BleHidCentral::matches(BLEAdvertisedDevice& device)
{
  return device.isConnectable() &&
         device.haveServiceUUID() &&
         device.isAdvertisingService(hidServiceUUID);
}

bool BleHidCentral::discover()
{
  clearDecoder();

  BLEClient* currentClient = client();
  if (currentClient == nullptr) return false;

  BLERemoteService* deviceInfoService = currentClient->getService(deviceInfoServiceUUID);
  if (deviceInfoService == nullptr) return false;

  // Add a new supported device by copying one tryMatch... helper and one line here.
  DecoderKind decoder = DecoderKind::None;
  uint16_t reportHandle = 0;

  if (!tryMatchMiniKeyboard(deviceInfoService, decoder, reportHandle) &&
      !tryMatchVol20(deviceInfoService, decoder, reportHandle))
    return false;

  BLERemoteService* hidService = currentClient->getService(hidServiceUUID);
  if (hidService == nullptr) return false;

  BLERemoteCharacteristic* report = findReportCharacteristic(hidService, reportHandle);
  if (report == nullptr) return false;

  activeInstance = this;
  decoder_ = decoder;
  reportHandle_ = reportHandle;

  if (!subscribeWithFilteredCccd(report, notifyCallback))
  {
    clearDecoder();
    if (activeInstance == this)
      activeInstance = nullptr;
    return false;
  }

  return true;
}

void BleHidCentral::resetPeerState()
{
  pendingState = {};
  clearDecoder();
  virtualPushUntil = 0;
  playPauseClickDeadline = 0;
  scanNextPressed = false;
  scanPreviousPressed = false;
  volumeIncrementPressed = false;
  volumeDecrementPressed = false;
  playPauseClickPending = false;
  playPausePressed = false;
  if (activeInstance == this)
    activeInstance = nullptr;
}

void BleHidCentral::notifyCallback(
  BLERemoteCharacteristic* characteristic,
  uint8_t* data,
  size_t length,
  bool isNotify)
{
  (void)isNotify;
  if (activeInstance && characteristic && characteristic->getUUID().equals(reportCharUUID))
    activeInstance->handleInputReport(characteristic, data, length);
}

void BleHidCentral::clearDecoder()
{
  decoder_ = DecoderKind::None;
  reportHandle_ = 0;
}

void BleHidCentral::handleInputReport(BLERemoteCharacteristic* characteristic, const uint8_t* data, size_t length)
{
  if (characteristic == nullptr || characteristic->getHandle() != reportHandle_) return;

  bool hasScanNext = false;
  bool hasScanPrevious = false;
  bool hasVolumeIncrement = false;
  bool hasVolumeDecrement = false;
  bool hasPlayPause = false;

  bool decoded = false;
  switch (decoder_)
  {
    case DecoderKind::ConsumerBitfield16:
      decoded = decodeConsumerBitfield16(
        data,
        length,
        hasScanNext,
        hasScanPrevious,
        hasVolumeIncrement,
        hasVolumeDecrement,
        hasPlayPause);
      break;

    case DecoderKind::ConsumerUsage16:
      decoded = decodeConsumerUsage16(
        data,
        length,
        hasScanNext,
        hasScanPrevious,
        hasVolumeIncrement,
        hasVolumeDecrement,
        hasPlayPause);
      break;

    case DecoderKind::None:
      return;
  }

  if (!decoded) return;

  if (hasVolumeIncrement && !volumeIncrementPressed && pendingState.rotation < 32767)
    pendingState.rotation++;

  if (hasVolumeDecrement && !volumeDecrementPressed && pendingState.rotation > -32768)
    pendingState.rotation--;

  if (hasScanNext && !scanNextPressed && pendingState.rotation < 32767)
  {
    pendingState.rotation++;
    holdVirtualPush();
  }

  if (hasScanPrevious && !scanPreviousPressed && pendingState.rotation > -32768)
  {
    pendingState.rotation--;
    holdVirtualPush();
  }

  if (!hasPlayPause && playPausePressed)
  {
    if (playPauseClickPending && (int32_t)(millis() - playPauseClickDeadline) < 0)
    {
      pendingState.wasShortPressed = true;
      playPauseClickPending = false;
      playPauseClickDeadline = 0;
    }
    else
    {
      playPauseClickPending = true;
      playPauseClickDeadline = millis() + playPauseDoubleClickMs;
    }
  }

  volumeIncrementPressed = hasVolumeIncrement;
  volumeDecrementPressed = hasVolumeDecrement;
  scanNextPressed = hasScanNext;
  scanPreviousPressed = hasScanPrevious;
  playPausePressed = hasPlayPause;
}

void BleHidCentral::holdVirtualPush()
{
  virtualPushUntil = millis() + virtualPushHoldMs;
}
