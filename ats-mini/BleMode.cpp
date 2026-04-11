#include "Common.h"
#include "Themes.h"
#include "Remote.h"
#include "Draw.h"
#include "BleHidCentral.h"
#include "BleMode.h"

static BleUartPeripheral BLESerial;
static BleHidCentral BLEHid;

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

void bleUpdate(uint8_t bleMode)
{
  if (bleMode == BLE_HID)
  {
    if (BLEHid.isStarted() && !BLEHid.isConnected() && BLEHid.pendingConnect && BLEHid.peerName())
    {
      drawScreen();
      drawScreen("Connecting BLE HID", BLEHid.peerName());
      delay(1000);
    }

    BLEHid.loop();
  }
}

bool bleIsPressed(uint8_t bleMode)
{
  return (bleMode == BLE_HID) && BLEHid.isConnected() && BLEHid.isPressed();
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

int bleDoCommand(RemoteState* state, uint8_t bleMode)
{
  if (bleMode == BLE_ADHOC)
  {
    if (!BLESerial.isConnected()) return 0;
    if (BLESerial.available())
      return remoteDoCommand(&BLESerial, state, BLESerial.read());
    return 0;
  }

  if (bleMode != BLE_HID) return 0;
  if (!BLEHid.isConnected()) return 0;

  BleKnobInput input;
  if (!BLEHid.read(input)) return 0;

  int event = REMOTE_CHANGED;
  if (input.rotation)
  {
    event |= input.rotation << REMOTE_DIRECTION;
    event |= REMOTE_PREFS;
  }
  if (input.clicked)
    event |= REMOTE_CLICK;
  if (input.shortPressed)
    event |= REMOTE_SHORT_PRESS;
  (void)state;
  return event;
}

void remoteBLETickTime(RemoteState* state, uint8_t bleMode)
{
  if (bleMode != BLE_ADHOC) return;
  if (BLESerial.isConnected())
    remoteTickTime(&BLESerial, state);
}
