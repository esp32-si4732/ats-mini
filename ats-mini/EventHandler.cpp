#include "Common.h"
#include "Rotary.h"
#include "Button.h"
#include "Menu.h"
#include "Utils.h"
#include "Remote.h"
#include "BleMode.h"
#include "Storage.h"
#include "EventHandler.h"

// Functions defined in ats-mini.ino
bool doTune(int16_t enc);
bool doDigit(int16_t enc);
bool doSeek(int16_t enc, int16_t enca);

// External variables defined in ats-mini.ino
extern volatile int16_t encoderCount;
extern volatile int16_t encoderCountAccel;
extern ButtonTracker pb1;
extern long elapsedSleep;
extern long elapsedCommand;

uint32_t consumeEncoderCounts()
{
  int16_t encCount, encCountAccel;
  noInterrupts();
  encCount = encoderCount;
  encCountAccel = encoderCountAccel;
  encoderCount = 0;
  encoderCountAccel = 0;
  interrupts();
  return ((uint32_t)encCountAccel << 16) | ((uint16_t)encCount & 0xFFFF);
}

bool handleEncoderInput(void)
{
  uint32_t currentTime = millis();
  bool needRedraw = false;

  uint32_t encCounts = consumeEncoderCounts();
  int16_t encCount = (int16_t)(encCounts & 0xFFFF);
  int16_t encCountAccel = (int16_t)(encCounts >> 16);

  ButtonTracker::State pb1st = pb1.update(digitalRead(ENCODER_PUSH_BUTTON) == LOW);

  // Receive and execute serial command
  int ser_event = serialLoop(radioState.usbMode);
  needRedraw |= !!(ser_event & REMOTE_CHANGED);
  pb1st.isPressed |= !!(ser_event & REMOTE_PRESSED);
  pb1st.wasClicked |= !!(ser_event & REMOTE_CLICK);
  pb1st.wasShortPressed |= !!(ser_event & REMOTE_SHORT_PRESS);
  int ser_direction = ser_event >> REMOTE_DIRECTION;
  encCount = ser_direction? ser_direction : encCount;
  encCountAccel = ser_direction? ser_direction : encCountAccel;
  if(ser_event & REMOTE_PREFS) prefsRequestSave(SAVE_ALL);

  // Receive and execute BLE command
  int ble_event = bleLoop(radioState.bleMode);
  needRedraw |= !!(ble_event & REMOTE_CHANGED);
  pb1st.isPressed |= !!(ble_event & REMOTE_PRESSED);
  pb1st.wasClicked |= !!(ble_event & REMOTE_CLICK);
  pb1st.wasShortPressed |= !!(ble_event & REMOTE_SHORT_PRESS);
  int ble_direction = ble_event >> REMOTE_DIRECTION;
  encCount = ble_direction? ble_direction : encCount;
  encCountAccel = ble_direction? ble_direction : encCountAccel;
  if(ble_event & REMOTE_PREFS) prefsRequestSave(SAVE_ALL);

  // Block encoder rotation when in the locked sleep mode
  if(encCount && sleepOn() && radioState.sleepMode==SLEEP_LOCKED) encCount = encCountAccel = 0;

  // Activate push and rotate mode (can span multiple loop iterations until the button is released)
  if (encCount && pb1st.isPressed) radioState.pnr = true;

  // Deactivate push and rotate mode as soon as the button is released so
  // click handling in this loop iteration follows the normal path.
  if(!pb1st.isPressed && radioState.pnr)
  {
    radioState.pnr = false;
    needRedraw = true;
  }

  // If push and rotate mode is active...
  if(radioState.pnr)
  {
    // If encoder has been rotated
    if(encCount)
    {
      switch(radioState.cmd)
      {
        case CMD_NONE:
          // Activate frequency input mode
          radioState.cmd = CMD_FREQ;
          needRedraw = true;
          break;
        case CMD_FREQ:
          // Select digit
          doSelectDigit(encCount);
          needRedraw = true;
          break;
        case CMD_SEEK:
          // Normal tuning in seek mode
          needRedraw |= doTune(encCount);
          // Current frequency may have changed
          prefsRequestSave(SAVE_CUR_BAND);
          break;
      }
    }
    // Reset timeouts while push and rotate is active
    elapsedSleep = elapsedCommand = currentTime;
  }
  else
  {
    // If encoder has been rotated
    if(encCount)
    {
      switch(radioState.cmd)
      {
        case CMD_NONE:
        case CMD_SCAN:
          // Tuning
          needRedraw |= doTune(encCountAccel);
          // Current frequency may have changed
          prefsRequestSave(SAVE_CUR_BAND);
          break;
        case CMD_FREQ:
          // Digit tuning
          needRedraw |= doDigit(encCount);
          // Current frequency may have changed
          prefsRequestSave(SAVE_CUR_BAND);
          break;
        case CMD_SEEK:
          // Seek mode
          needRedraw |= doSeek(encCount, encCountAccel);
          // Seek can take long time, renew the timestamp
          currentTime = millis();
          // Current frequency may have changed
          prefsRequestSave(SAVE_CUR_BAND);
          break;
        default:
          // Side bar menus / settings
          needRedraw |= doSideBar(radioState.cmd, encCount, encCountAccel);
          // Current settings, etc. may have changed
          prefsRequestSave(SAVE_ALL);
          break;
      }

      // Reset timeouts
      elapsedSleep = elapsedCommand = currentTime;
    }
    else if(pb1st.isLongPressed)
    {
      // Encoder is being LONG PRESSED: TOGGLE DISPLAY
      sleepOn(!sleepOn());
      // CPU sleep can take long time, renew the timestamps
      elapsedSleep = elapsedCommand = currentTime = millis();

    }
    else if(pb1st.wasClicked || pb1st.wasShortPressed)
    {
      // Encoder click or short press
      // Reset timeouts
      elapsedSleep = elapsedCommand = currentTime;

      // If in locked/unlocked sleep mode
      if(sleepOn())
      {
        // If sleep timeout is enabled, exit it via button press of any duration
        // (users don't need to figure out that a long press is required to wake up the device)
        if(radioState.sleep)
        {
          sleepOn(false);
          needRedraw = true;
        }
        else if(radioState.sleepMode == SLEEP_UNLOCKED)
        {
          // Allow to adjust the volume in sleep mode
          if(pb1st.wasShortPressed && radioState.cmd==CMD_NONE)
            radioState.cmd = CMD_VOLUME;
          else if(radioState.cmd==CMD_VOLUME)
            clickHandler(radioState.cmd, pb1st.wasShortPressed);

          needRedraw = true;
        }
      }
      else if(clickHandler(radioState.cmd, pb1st.wasShortPressed))
      {
        // Command handled, redraw screen
        needRedraw = true;

        // EiBi can take long time, renew the timestamps
        elapsedSleep = elapsedCommand = currentTime = millis();
      }
      else if(radioState.cmd != CMD_NONE)
      {
        // Deactivate modal mode
        radioState.cmd = CMD_NONE;
        needRedraw = true;
      }
      else if(pb1st.wasShortPressed)
      {
        // Volume shortcut (only active in VFO mode)
        radioState.cmd = CMD_VOLUME;
        needRedraw = true;
      }
      else
      {
        // Activate menu
        radioState.cmd = CMD_MENU;
        needRedraw = true;
      }
    }
  }

  return needRedraw;
}
