#include "Common.h"
#include "Themes.h"
#include "Remote.h"
#include "Draw.h"
#include "BleHidCentral.h"
#include "BleMode.h"

static BleUartPeripheral BLESerial;
static BleHidCentral BLEHid;
static RemoteState remoteBLEState;

static void bleControllerDisable()
{
  if (BLEDevice::getInitialized() && esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED)
    esp_bt_controller_disable();
}

static bool bleControllerEnable()
{
  if (!BLEDevice::getInitialized()) return true;

  esp_bt_controller_status_t status = esp_bt_controller_get_status();
  if (status == ESP_BT_CONTROLLER_STATUS_ENABLED) return true;
  if (status == ESP_BT_CONTROLLER_STATUS_INITED)
    return esp_bt_controller_enable(ESP_BT_MODE_BLE) == ESP_OK;

  return false;
}

//
// Get current connection status
// (-1 - not connected, 0 - disabled, 1 - connected)
//
int8_t getBleStatus()
{
  if (BLESerial.isStarted())
    return BLESerial.isConnected() ? 1 : -1;

  if (BLEHid.isStarted())
    return BLEHid.isConnected() ? 1 : -1;

  return 0;
}

//
// Stop BLE hardware
//
// We do not deinit the BLE stack fully because it leads to crashes
// Also see: https://github.com/afpineda/NuS-NimBLE-Serial/issues/12#issuecomment-2912732754
//
void bleStop()
{
  if (BLESerial.isStarted())
    BLESerial.end();

  if (BLEHid.isStarted())
    BLEHid.end();

  // Experimental attempt to stop consuming power
  // https://github.com/esp32-si4732/ats-mini/discussions/324#discussioncomment-17123023
  bleControllerDisable();
}

void bleInit(uint8_t bleMode)
{
  bleStop();
  if (bleMode != BLE_OFF && !bleControllerEnable()) return;

  switch(bleMode)
  {
    case BLE_ADHOC:
      BLESerial.begin(RECEIVER_NAME);
      break;
    case BLE_HID:
      BLEHid.begin(RECEIVER_NAME);
      break;
  }
}

int bleLoop(uint8_t bleMode)
{
  if (bleMode == BLE_ADHOC)
  {
    if (BLESerial.isConnected())
      remoteTickTime(&BLESerial, &remoteBLEState);
    if (!BLESerial.isConnected()) return 0;
    if (BLESerial.available())
      return remoteDoCommand(&BLESerial, &remoteBLEState, BLESerial.read());
    return 0;
  }

  if (bleMode != BLE_HID)
    return 0;

  if (BLEHid.isStarted() && !BLEHid.isConnected() && BLEHid.isConnectPending() && BLEHid.peerName())
  {
    drawScreen();
    drawScreen("Connecting BLE HID", BLEHid.peerName());
    delay(500);
  }

  BLEHid.loop();
  if (!BLEHid.isConnected()) return 0;

  BleHidState input = BLEHid.update();
  int event = input.isPressed ? REMOTE_PRESSED : 0;
  if (!input.rotation && !input.wasClicked && !input.wasShortPressed) return event;

  event |= REMOTE_CHANGED;
  if (input.rotation)
  {
    event |= input.rotation << REMOTE_DIRECTION;
    event |= REMOTE_PREFS;
  }
  if (input.wasClicked)
    event |= REMOTE_CLICK;
  if (input.wasShortPressed)
    event |= REMOTE_SHORT_PRESS;
  return event;
}

bool bleConsumeAbortPending(uint8_t bleMode)
{
  if (bleMode == BLE_ADHOC)
    return BLESerial.consumeAbortPending();

  if (bleMode == BLE_HID)
    return BLEHid.consumeAbortPending();

  return false;
}
