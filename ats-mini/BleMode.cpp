#include "Common.h"
#include "Themes.h"
#include "Remote.h"
#include "Draw.h"
#include "BleHidCentral.h"
#include "BleMode.h"

static BleUartPeripheral BLESerial;
static BleHidCentral BLEHid;
static RemoteState remoteBLEState;

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
void bleStop()
{
  if (BLESerial.isStarted())
    BLESerial.end();

  if (BLEHid.isStarted())
    BLEHid.end();
}

void bleInit(uint8_t bleMode)
{
  bleStop();

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
