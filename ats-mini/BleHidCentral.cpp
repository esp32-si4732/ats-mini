#include "BleHidCentral.h"
#include "Draw.h"
#include <string.h>

static BLEUUID hidServiceUUID((uint16_t)0x1812);
static BLEUUID reportCharUUID((uint16_t)0x2A4D);

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
  return device.haveServiceUUID() && device.isAdvertisingService(hidServiceUUID);
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

  for (auto const& entry : *characteristics)
  {
    BLERemoteCharacteristic* characteristic = entry.second;
    if (!characteristic->getUUID().equals(reportCharUUID) || !characteristic->canNotify()) continue;

    characteristic->registerForNotify(notifyCallback);
    subscribed = true;
  }

  if (!subscribed) return false;
  activeInstance = this;
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
