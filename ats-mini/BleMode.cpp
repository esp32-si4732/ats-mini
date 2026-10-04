#include "Common.h"
#include "Themes.h"
#include "Remote.h"
#include "Draw.h"
#include "Utils.h"
#include "Storage.h"
#include "BleHidCentral.h"
#include "BleMode.h"
#include <host/ble_store.h>

static BleUartPeripheral BLESerial;
static BleHidCentral BLEHid;
static RemoteState remoteBLEState;
static bool hidPushAndRotate = false;

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
  hidPushAndRotate = false;
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

  if (bleMode == BLE_UNPAIR_ALL)
  {
    // The BLE store is initialized lazily, so initialize it even when the
    // receiver was previously in Off mode.
    if (!BLEDevice::getInitialized())
      BLEDevice::init(RECEIVER_NAME);

    if (BLEDevice::getInitialized())
      ble_store_clear();

    bleControllerDisable();
    bleModeIdx = BLE_OFF;
    prefsRequestSave(SAVE_SETTINGS, true);
    return;
  }

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

static InputEvent bleReadInput(uint8_t& byte)
{
  return BLESerial.readInput(byte);
}

bool blePushAndRotate()
{
  return hidPushAndRotate;
}

bool bleLoop(uint8_t bleMode)
{
  if(bleMode != BLE_HID) hidPushAndRotate = false;
  if (bleMode == BLE_ADHOC)
    return remoteLoop(&BLESerial, &remoteBLEState, bleReadInput);

  if (bleMode != BLE_HID)
    return false;

  // BLEHid.loop() below connects synchronously, so the status stays up for
  // the whole connection attempt
  if (BLEHid.isConnectPending() && BLEHid.peerName())
  {
    statusShow("Connecting BLE HID", BLEHid.peerName(), 0);
    drawScreen();
    BLEHid.loop();
    statusShow(nullptr);
  }
  else
    BLEHid.loop();

  if (!BLEHid.isConnected())
  {
    hidPushAndRotate = false;
    return false;
  }

  BleHidState input = BLEHid.update();
  if(!input.isPressed) hidPushAndRotate = false;
  else if(input.rotation && !(sleepOn() && sleepModeIdx == SLEEP_LOCKED))
    hidPushAndRotate = true;
  if(input.rotation)
    return handleRotation(input.rotation, input.rotation, input.isPressed);
  if(input.wasClicked || input.wasShortPressed)
    return handleClick(input.wasShortPressed);
  return false;
}

bool bleConsumeAbortPending(uint8_t bleMode)
{
  if (bleMode == BLE_ADHOC)
    return remoteConsumeAbortPending(&remoteBLEState, bleReadInput);

  if (bleMode == BLE_HID && BLEHid.consumeAbortPending())
  {
    hidPushAndRotate = false;
    return true;
  }

  return false;
}
