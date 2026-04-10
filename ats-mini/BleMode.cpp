#include "Common.h"
#include "Themes.h"
#include "Remote.h"
#include "BleMode.h"

BlePeripheral BLESerial;

//
// Get current connection status
// (-1 - not connected, 0 - disabled, 1 - connected)
//
int8_t getBleStatus()
{
  if(!BLESerial.isStarted()) return 0;
  return BLESerial.isConnected() ? 1 : -1;
}

//
// Stop BLE hardware
//
void bleStop()
{
  if(!BLESerial.isStarted()) return;
  BLESerial.end();
}

void bleInit(uint8_t bleMode)
{
  bleStop();

  if(bleMode == BLE_OFF) return;
  BLESerial.begin(RECEIVER_NAME);
}

int bleDoCommand(Stream* stream, RemoteState* state, uint8_t bleMode)
{
  if(bleMode == BLE_OFF) return 0;

  if (BLESerial.isConnected()) {
//    if (BLESerial.available())
//      return remoteDoCommand(stream, state, BLESerial.read());
  }
  return 0;
}

void remoteBLETickTime(Stream* stream, RemoteState* state, uint8_t bleMode)
{
  if(bleMode == BLE_OFF) return;

//  if (BLESerial.isConnected())
//    remoteTickTime(stream, state);
}
