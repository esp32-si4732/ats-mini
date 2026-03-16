#include "Common.h"
#include "Themes.h"
#include "Remote.h"
#include "Ble.h"

//
// Get current connection status
// (-1 - not connected, 0 - disabled, 1 - connected)
//
int8_t getBleStatus()
{
  if(!BLESerial.isStarted()) return 0;
  return BLEDevice::getServer()->getConnectedCount() > 0 ? 1 : -1;
}

//
// Stop BLE hardware
//
void bleStop()
{
  if(!BLESerial.isStarted()) return;
  BLESerial.stop();
}

void bleInit(uint8_t bleMode)
{
  bleStop();

  if(bleMode == BLE_OFF) return;
  BLESerial.start();
  // Send a JSON status greeting once a client connects (handled by onConnect callback).
  // The auto-off timer in ats-mini.ino seeds itself on the first loop iteration.
}

int bleDoCommand(Stream* stream, RemoteState* state, uint8_t bleMode)
{
  if(bleMode == BLE_OFF) return 0;
  if(BLEDevice::getServer()->getConnectedCount() == 0) return 0;
  if(!BLESerial.available()) return 0;

  // JSON command: peek for opening brace, read full packet, dispatch
  if(BLESerial.peek() == '{')
  {
    static char buf[512];
    int len = 0;
    while(BLESerial.available() && len < (int)sizeof(buf) - 1)
      buf[len++] = BLESerial.read();
    buf[len] = '\0';
    return remoteDoJsonCommand(stream, state, buf);
  }

  // Legacy single-char command
  return remoteDoCommand(stream, state, BLESerial.read());
}

void remoteBLETickTime(Stream* stream, RemoteState* state, uint8_t bleMode)
{
  if(bleMode == BLE_OFF) return;

  if (BLEDevice::getServer()->getConnectedCount() > 0)
    remoteTickTime(stream, state);
}
