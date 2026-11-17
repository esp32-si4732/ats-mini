#include "Common.h"
#include "Remote.h"
#include "UsbMode.h"

static RemoteState remoteSerialState;
static bool serialDisconnectPending = false;
static uint32_t serialDisconnectedAt;

static InputEvent serialReadInput(uint8_t& byte)
{
  // HWCDC can briefly report a false disconnect. Queued input takes priority;
  // only a sustained disconnect without input should erase a partial command.
  if(!Serial && !Serial.available())
  {
    if(!serialDisconnectPending)
    {
      serialDisconnectedAt = millis();
      serialDisconnectPending = true;
    }
    return millis() - serialDisconnectedAt >= 1000
      ? InputEvent::SessionReset : InputEvent::NoData;
  }
  serialDisconnectPending = false;
  int value = Serial.read();
  if(value < 0) return InputEvent::NoData;
  byte = uint8_t(value);
  return InputEvent::Byte;
}

bool usbLoop(uint8_t usbMode)
{
  if(usbMode == USB_OFF)
  {
    remoteSerialState = RemoteState{};
    serialDisconnectPending = false;
    return false;
  }
  return remoteLoop(&Serial, &remoteSerialState, serialReadInput);
}

bool usbConsumeAbortPending(uint8_t usbMode)
{
  return usbMode != USB_OFF && remoteConsumeAbortPending(&remoteSerialState, serialReadInput);
}
