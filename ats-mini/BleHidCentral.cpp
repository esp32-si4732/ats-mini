#include <functional>
#include <map>
#include <string>

// Arduino-ESP32's NimBLE wrapper leaks BLERemoteDescriptor objects when HID notify
// subscription goes through the normal public path:
//   registerForNotify() -> subscribe() -> setNotify() -> retrieveDescriptors()
// The wrapper scans descriptors from the current characteristic handle to the end
// of the whole service, allocates descriptor objects before inserting them into a
// UUID-keyed map, and orphaned duplicates are never reclaimed.
//
// We keep the hack confined to this translation unit and use it only to call the
// private filtered descriptor lookup path with UUID 0x2902 (CCCD) before the public
// subscribe() call. That forces the wrapper onto its early-exit path and avoids the
// broad leaking descriptor walk.
#define private public
#include <BLEClient.h>
#undef private

#include "BleHidCentral.h"
#include "Draw.h"
#include <string.h>

static BLEUUID hidServiceUUID((uint16_t)0x1812);
static BLEUUID reportCharUUID((uint16_t)0x2A4D);
static BLEUUID cccdUUID((uint16_t)0x2902);

static constexpr uint16_t consumerUsagePlayPause = 0x00CD;
static constexpr uint16_t consumerUsageScanNextTrack = 0x00B5;
static constexpr uint16_t consumerUsageScanPreviousTrack = 0x00B6;
static constexpr uint16_t consumerUsageVolumeIncrement = 0x00E9;
static constexpr uint16_t consumerUsageVolumeDecrement = 0x00EA;
static constexpr uint16_t consumerBitsVolumeDecrement = 1u << 0;
static constexpr uint16_t consumerBitsVolumeIncrement = 1u << 1;
static constexpr uint16_t consumerBitsScanPreviousTrack = 1u << 3;
static constexpr uint16_t consumerBitsScanNextTrack = 1u << 4;
static constexpr uint16_t consumerBitsPlayPause = 1u << 5;

BleHidCentral* BleHidCentral::activeInstance = nullptr;

static bool subscribeWithFilteredCccd(BLERemoteCharacteristic* characteristic, notify_callback callback)
{
  if (characteristic == nullptr) return false;

  std::string cccdKey = cccdUUID.toString().c_str();
  auto hasCccd = [&]() {
    return characteristic->m_descriptorMap.find(cccdKey) != characteristic->m_descriptorMap.end();
  };

  // Preload only the CCCD so the later public subscribe() call skips the wrapper's
  // broad descriptor discovery path. If the wrapper already marked descriptors as
  // retrieved without retaining a CCCD entry, reset and force a filtered lookup.
  if (!hasCccd())
  {
    characteristic->removeDescriptors();
    if (!characteristic->retrieveDescriptors(&cccdUUID) || !hasCccd())
      return false;
  }

  if (!characteristic->subscribe(true, callback, true))
    return false;

  return hasCccd();
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
  BLEClient* currentClient = client();
  if (currentClient == nullptr) return false;

  BLERemoteService* hidService = currentClient->getService(hidServiceUUID);
  if (hidService == nullptr) return false;

  bool subscribed = false;
  std::map<uint16_t, BLERemoteCharacteristic*>* characteristics = nullptr;
  hidService->getCharacteristics(&characteristics);
  if (characteristics == nullptr) return false;

  // Notifications can arrive immediately after the first CCCD write, so arm the
  // active instance before we start subscribing and clear it again on failure.
  activeInstance = this;

  for (auto const& entry : *characteristics)
  {
    BLERemoteCharacteristic* characteristic = entry.second;
    if (!characteristic->getUUID().equals(reportCharUUID) || !characteristic->canNotify()) continue;

    if (!subscribeWithFilteredCccd(characteristic, notifyCallback))
    {
      if (activeInstance == this)
        activeInstance = nullptr;
      return false;
    }
    subscribed = true;
  }

  if (!subscribed)
  {
    if (activeInstance == this)
      activeInstance = nullptr;
    return false;
  }

  return true;
}

void BleHidCentral::resetPeerState()
{
  pendingState = {};
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
    activeInstance->handleInputReport(data, length);
}

void BleHidCentral::handleInputReport(const uint8_t* data, size_t length)
{
  bool hasScanNext = false;
  bool hasScanPrevious = false;
  bool hasVolumeIncrement = false;
  bool hasVolumeDecrement = false;
  bool hasPlayPause = false;

  if (length == 2)
  {
    uint16_t usage = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    hasScanNext = usage == consumerUsageScanNextTrack;
    hasScanPrevious = usage == consumerUsageScanPreviousTrack;
    hasVolumeIncrement = usage == consumerUsageVolumeIncrement;
    hasVolumeDecrement = usage == consumerUsageVolumeDecrement;
    hasPlayPause = usage == consumerUsagePlayPause;
  }
  else if (length == 3)
  {
    uint16_t bits = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    hasScanNext = (bits & consumerBitsScanNextTrack) != 0;
    hasScanPrevious = (bits & consumerBitsScanPreviousTrack) != 0;
    hasVolumeIncrement = (bits & consumerBitsVolumeIncrement) != 0;
    hasVolumeDecrement = (bits & consumerBitsVolumeDecrement) != 0;
    hasPlayPause = (bits & consumerBitsPlayPause) != 0;
  }
  else
    return;

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
