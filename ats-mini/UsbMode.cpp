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

static bool serialSelectMode(uint8_t usbMode)
{
  static uint8_t previousMode = USB_OFF;
  if(usbMode != previousMode)
  {
    remoteSerialState = RemoteState{};
    serialDisconnectPending = false;
    // Bytes queued under the previous protocol must not become new commands.
    int pending = Serial.available();
    while(pending-- > 0) Serial.read();
    previousMode = usbMode;
  }
  return usbMode == USB_ADHOC || usbMode == USB_KENWOOD;
}

bool usbLoop(uint8_t usbMode)
{
  if(!serialSelectMode(usbMode)) return false;
  return remoteLoop(Serial, remoteSerialState, serialReadInput,
                    usbMode == USB_KENWOOD ? RemoteProtocol::Kenwood : RemoteProtocol::Adhoc);
}

bool usbConsumeAbortPending(uint8_t usbMode)
{
  if(!serialSelectMode(usbMode)) return false;
  return remoteConsumeAbortPending(remoteSerialState, serialReadInput,
                                   usbMode == USB_KENWOOD ? RemoteProtocol::Kenwood : RemoteProtocol::Adhoc);
}
