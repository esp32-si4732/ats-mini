// =================================
// INCLUDE FILES
// =================================

#include <Wire.h>
#include <TFT_eSPI.h>            // https://github.com/Xinyuan-LilyGO/T-Display-S3#quick-start
#include "EEPROM.h"
#include <SI4735.h>
#include "Rotary.h"              // Disabled half-step mode
#include "Common.h"
#include "Menu.h"
#include "Storage.h"
#include "Themes.h"

// SI473/5 and UI
#define MIN_ELAPSED_TIME         5  // 300
#define MIN_ELAPSED_RSSI_TIME  200  // RSSI check uses IN_ELAPSED_RSSI_TIME * 6 = 1.2s
#define ELAPSED_COMMAND      10000  // time to turn off the last command controlled by encoder. Time to goes back to the VFO control // G8PTN: Increased time and corrected comment
#define DEFAULT_VOLUME          35  // change it for your favorite sound volume
#define STRENGTH_CHECK_TIME   1500  // Not used
#define RDS_CHECK_TIME         250  // Increased from 90
#define CLICK_TIME              50
#define SHORT_PRESS_TIME       500
#define LONG_PRESS_TIME        2000

#define BACKGROUND_REFRESH_TIME 5000    // Background screen refresh time. Covers the situation where there are no other events causing a refresh
#define TUNE_HOLDOFF_TIME         90    // Timer to hold off display whilst tuning

// =================================
// CONSTANTS AND VARIABLES
// =================================

int8_t agcIdx = 0;
uint8_t disableAgc = 0;
int8_t agcNdx = 0;
int8_t softMuteMaxAttIdx = 4;

uint8_t seekDirection = 1;
bool seekStop = false;        // G8PTN: Added flag to abort seeking on rotary encoder detection
bool seekModePress = false;   // Seek happened during long press

long elapsedRSSI = millis();
long elapsedButton = millis();

long lastStrengthCheck = millis();
long lastRDSCheck = millis();

long elapsedCommand = millis();
volatile int encoderCount = 0;
uint16_t currentFrequency;

// AGC/ATTN index per mode (FM/AM/SSB)
int8_t FmAgcIdx = 0;                    // Default FM  AGGON  : Range = 0 to 37, 0 = AGCON, 1 - 27 = ATTN 0 to 26
int8_t AmAgcIdx = 0;                    // Default AM  AGCON  : Range = 0 to 37, 0 = AGCON, 1 - 37 = ATTN 0 to 36
int8_t SsbAgcIdx = 0;                   // Default SSB AGCON  : Range = 0 to 1,  0 = AGCON,      1 = ATTN 0

// AVC index per mode (AM/SSB)
int8_t AmAvcIdx = 48;                   // Default AM  = 48 (as per AN332), range = 12 to 90 in steps of 2
int8_t SsbAvcIdx = 48;                  // Default SSB = 48, range = 12 to 90 in steps of 2

// SoftMute index per mode (AM/SSB)
int8_t AmSoftMuteIdx = 4;               // Default AM  = 4, range = 0 to 32
int8_t SsbSoftMuteIdx = 4;              // Default SSB = 4, range = 0 to 32

// Button checking
unsigned long pb1_time = 0;             // Push button timer
unsigned long pb1_edge_time = 0;        // Push button edge time
unsigned long pb1_pressed_time = 0;     // Push button pressed time
unsigned long pb1_short_pressed_time = 0; // Push button short pressed time
unsigned long pb1_long_pressed_time = 0;// Push button long pressed time
unsigned long pb1_released_time = 0;    // Push button released time
int pb1_current = HIGH;                 // Push button current state
int pb1_stable = HIGH;                  // Push button stable state
int pb1_last = HIGH;                    // Push button last state (after debounce)
bool pb1_pressed = false;               // Push button pressed
bool pb1_short_pressed = false;         // Push button short pressed
bool pb1_long_pressed = false;          // Push button long pressed
bool pb1_released = false;              // Push button released
bool pb1_short_released = false;        // Push button short released
bool pb1_long_released = false;         // Push button long released

// Status bar icon flags
bool screen_toggle = false;             // Toggle when drawsprite is called

// Menu options
uint16_t currentBrt = 128;              // Display brightness, range = 32 to 255 in steps of 32
uint16_t currentSleep = 30;             // Display sleep timeout, range = 0 to 255 in steps of 5
long elapsedSleep = millis();           // Display sleep timer

// Background screen refresh
uint32_t background_timer = millis();   // Background screen refresh timer.
uint32_t tuning_timer = millis();       // Tuning hold off timer.
bool tuning_flag = false;               // Flag to indicate tuning

//
// Current parameters
//
uint16_t currentCmd  = CMD_NONE;
uint8_t  currentMode = FM;
int16_t  currentBFO  = 0;

uint8_t  rssi = 0;
uint8_t  snr  = 0;

//
// Devices
//
Rotary encoder  = Rotary(ENCODER_PIN_B, ENCODER_PIN_A);
TFT_eSPI tft    = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);
SI4735 rx;

//
// Hardware initialization and setup
//
void setup()
{
  // Enable serial port
  Serial.begin(115200);

  // Enable audio amplifier
  // Initally disable the audio amplifier until the SI4732 has been setup
  pinMode(PIN_AMP_EN, OUTPUT);
  digitalWrite(PIN_AMP_EN, LOW);

  // Enable SI4732 VDD
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);

  // Encoder pins. Enable internal pull-ups
  pinMode(ENCODER_PUSH_BUTTON, INPUT_PULLUP);
  pinMode(ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(ENCODER_PIN_B, INPUT_PULLUP);

  // The line below may be necessary to setup I2C pins on ESP32
  Wire.begin(ESP32_I2C_SDA, ESP32_I2C_SCL);

  // TFT display setup
  tft.begin();
  tft.setRotation(3);

  // Detect and fix the mirrored & inverted display
  // https://github.com/esp32-si4732/ats-mini/issues/41
  if(tft.readcommand8(ST7789_RDDID, 3) == 0x93)
  {
    tft.invertDisplay(0);
    tft.writecommand(TFT_MADCTL);
    tft.writedata(TFT_MAD_MV | TFT_MAD_MX | TFT_MAD_MY | TFT_MAD_COLOR_ORDER);
  }

  tft.fillScreen(TH.bg);
  spr.createSprite(320, 170);
  spr.setTextDatum(MC_DATUM);
  spr.setSwapBytes(true);
  spr.setFreeFont(&Orbitron_Light_24);
  spr.setTextColor(TH.text, TH.bg);

  // TFT display brightness control (PWM)
  // Note: At brightness levels below 100%, switching from the PWM may cause power spikes and/or RFI
  ledcAttach(PIN_LCD_BL, 16000, 8);  // Pin assignment, 16kHz, 8-bit
//  ledcWrite(PIN_LCD_BL, 255);        // Default value 255 = 100%
  ledcWrite(PIN_LCD_BL, 0);          // Turn brightness off for now

  // Press and hold Encoder button to force an EEPROM reset
  // Note: EEPROM reset is recommended after firmware updates
  if(digitalRead(ENCODER_PUSH_BUTTON)==LOW)
  {
    eepromInvalidate();

    tft.setTextSize(2);
    tft.setTextColor(TH.text, TH.bg);
    tft.println(getVersion());
    tft.println();
    tft.setTextColor(TH.text_warn, TH.bg);
    tft.print("EEPROM Resetting");
    delay(2000);
  }

  // G8PTN: Moved this to later, to avoid interrupt action
  /*
  // ICACHE_RAM_ATTR void rotaryEncoder(); see rotaryEncoder implementation below.
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), rotaryEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), rotaryEncoder, CHANGE);
  */

  // Check for SI4732 connected on I2C interface
  // If the SI4732 is not detected, then halt with no further processing
  rx.setI2CFastModeCustom(100000);

  // Looks for the I2C bus address and set it.  Returns 0 if error
  int16_t si4735Addr = rx.getDeviceI2CAddress(RESET_PIN);
  if(!si4735Addr)
  {
    tft.setTextSize(2);
    tft.setTextColor(TH.text_warn, TH.bg);
    tft.println("Si4732 not detected");
    while(1);
  }

  rx.setup(RESET_PIN, MW_BAND_TYPE);
  // Comment the line above and uncomment the three lines below if you are using external ref clock (active crystal or signal generator)
  // rx.setRefClock(32768);
  // rx.setRefClockPrescaler(1);   // will work with 32768
  // rx.setup(RESET_PIN, 0, MW_BAND_TYPE, SI473X_ANALOG_AUDIO, XOSCEN_RCLK);

  // Attached pin to allows SI4732 library to mute audio as required to minimise loud clicks
  rx.setAudioMuteMcuPin(AUDIO_MUTE);

  clearStationName();

  delay(300);

  // Audio Amplifier Enable. G8PTN: Added
  // After the SI4732 has been setup, enable the audio amplifier
  digitalWrite(PIN_AMP_EN, HIGH);

  // If EEPROM contents are ok...
  if(eepromVerify())
  {
    // Load configuration from EEPROM
    eepromLoadConfig();
  }
  else
  {
    // Save default configuration to EEPROM
    eepromSaveConfig();
    // Set initial volume after EEPROM reset
    rx.setVolume(DEFAULT_VOLUME);
    // Set initial brightness after EEPROM reset
    ledcWrite(PIN_LCD_BL, currentBrt);
  }

  // ** SI4732 STARTUP **
  // Uses values from EEPROM (Last stored or defaults after EEPROM reset)
  selectBand(bandIdx);

  // Draw display for the first time
  drawScreen();

  // Interrupt actions for Rotary encoder
  // Note: Moved to end of setup to avoid inital interrupt actions
  // ICACHE_RAM_ATTR void rotaryEncoder(); see rotaryEncoder implementation below.
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), rotaryEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), rotaryEncoder, CHANGE);
}

//
// Reads encoder via interrupt
// Uses Rotary.h and Rotary.cpp implementation to process encoder via
// interrupt. If you do not add ICACHE_RAM_ATTR declaration, the system
// will reboot during attachInterrupt call. The ICACHE_RAM_ATTR macro
// places this function into RAM.
//
ICACHE_RAM_ATTR void rotaryEncoder()
{
  // Rotary encoder events
  uint8_t encoderStatus = encoder.process();
  if(encoderStatus)
  {
    encoderCount = encoderStatus==DIR_CW? 1 : -1;
    seekStop = true;
  }
}

//
// Switch radio to given band
//
void useBand(const Band *band)
{
  // Set current frequency and mode, reset BFO
  currentFrequency = band->currentFreq;
  currentMode = band->bandMode;
  currentBFO = 0;

  if(currentMode==FM)
  {
    // rx.setTuneFrequencyAntennaCapacitor(0);
    rx.setFM(band->minimumFreq, band->maximumFreq, band->currentFreq, getCurrentStep()->step);
    rx.setSeekFmLimits(band->minimumFreq, band->maximumFreq);
    rx.setFMDeEmphasis(1);
    rx.RdsInit();
    rx.setRdsConfig(1, 2, 2, 2, 2);
    rx.setGpioCtl(1, 0, 0);   // G8PTN: Enable GPIO1 as output
    rx.setGpio(0, 0, 0);      // G8PTN: Set GPIO1 = 0
  }
  else
  {
    // Set the tuning capacitor for SW or MW/LW
    // rx.setTuneFrequencyAntennaCapacitor((band[bandIdx].bandType == MW_BAND_TYPE || band[bandIdx].bandType == LW_BAND_TYPE) ? 0 : 1);

    if(isSSB())
    {
      // Configure SI4732 for SSB (SI4732 step not used, set to 0)
      rx.setSSB(band->minimumFreq, band->maximumFreq, band->currentFreq, 0, currentMode);
      // G8PTN: Always enabled
      rx.setSSBAutomaticVolumeControl(1);
      // G8PTN: Commented out
      //rx.setSsbSoftMuteMaxAttenuation(softMuteMaxAttIdx);
    }
    else
    {
      // Setting step to 1kHz
      int step = band->currentStepIdx>=AmTotalSteps? 1 : getCurrentStep()->step;
      rx.setAM(band->minimumFreq, band->maximumFreq, band->currentFreq, step);
    }

    // G8PTN: Enable GPIO1 as output
    rx.setGpioCtl(1, 0, 0);
    // G8PTN: Set GPIO1 = 1
    rx.setGpio(1, 0, 0);
    // Consider the range all defined current band
    rx.setSeekAmLimits(band->minimumFreq, band->maximumFreq);
    // Max 10kHz for spacing
    rx.setSeekAmSpacing(5);
  }

  // Set softMuteMaxAttIdx based on mode (AM, SSB)
  doSoftMute(0);
  // Set disableAgc and agcNdx values based on mode (FM, AM , SSB)
  doAgc(0);
  // Set currentAVC values based on mode (AM, SSB)
  doAvc(0);
  // Wait a bit for things to calm down
  delay(100);
  // Clear signal strength readings
  rssi = 0;
  snr  = 0;
  // Clear current station name (RDS/CB)
  clearStationName();
}

// This function is called by the seek function process.
bool checkStopSeeking()
{
  // Returns true if the user rotates the encoder
  return(seekStop);
}

// This function is called by the seek function process.
void showFrequencySeek(uint16_t freq)
{
  currentFrequency = freq;
  drawScreen();
}

//
// Find a station. The direction is based on the last encoder move
// clockwise or counterclockwise
//
void doSeek()
{
  // It does not work for SSB mode
  if(isSSB()) return;

  rx.seekStationProgress(showFrequencySeek, checkStopSeeking, seekDirection);   // G8PTN: Added checkStopSeeking
  currentFrequency = rx.getFrequency();
}

//
// Tune using BFO, using algorithm from Goshante's ATS-20_EX firmware
//
void updateBFO(int newBFO)
{
  Band *band = getCurrentBand();
  int newFreq = currentFrequency;

  // No BFO outside SSB modes
  if(!isSSB()) newBFO = 0;

  // If new BFO exceeds allowed bounds...
  if(newBFO > MAX_BFO || newBFO < -MAX_BFO)
  {
    // Compute correction
    int fCorrect = (newBFO / MAX_BFO) * MAX_BFO;
    // Correct new frequency and BFO
    newFreq += fCorrect / 1000;
    newBFO  -= fCorrect;
  }

  // Do not let new frequency exceed band limits
  int f = newFreq * 1000 + newBFO;
  if(f < band->minimumFreq * 1000)
  {
    newFreq = band->maximumFreq;
    newBFO  = 0;
  }
  else if(f > band->maximumFreq * 1000)
  {
    newFreq = band->minimumFreq;
    newBFO  = 0;
  }

  // If need to change frequency...
  if(newFreq != currentFrequency)
  {
    // Apply new frequency
    rx.setFrequency(newFreq);
    // Re-apply to remove noise
    doAgc(0);
    // Update current frequency
    currentFrequency = rx.getFrequency();
  }

  // Update current BFO
  currentBFO = newBFO;

  // To move frequency forward, need to move the BFO backwards
  rx.setSSBBfo(-(currentBFO + band->bandCal));

  // Save current band frequency, w.r.t. new BFO value
  band->currentFreq = currentFrequency + currentBFO / 1000;
}

//
// Tune to a new frequency, resetting BFO if present
//
void updateFrequency(int newFreq)
{
  Band *band = getCurrentBand();

  // Do not let new frequency exceed band limits
  newFreq = newFreq<band->minimumFreq? band->maximumFreq
          : newFreq>band->maximumFreq? band->minimumFreq
          : newFreq;

  // Set new frequency
  rx.setFrequency(newFreq);

  // Clear BFO, if present
  if(currentBFO) updateBFO(0);

  // Update current frequency
  currentFrequency = rx.getFrequency();

  // Save current band frequency
  band->currentFreq = currentFrequency + currentBFO / 1000;
}

void buttonCheck()
{
  // Push button detection, only execute every 10 ms
  if((millis() - pb1_time) > 10)
  {
    pb1_time = millis();

    // Read pin value
    pb1_current = digitalRead(ENCODER_PUSH_BUTTON);

    // Start debounce timer
    if(pb1_last != pb1_current)
    {
      pb1_edge_time = millis();
      pb1_last = pb1_current;
    }

    // Debounced
    if((millis() - pb1_edge_time) > CLICK_TIME)
    {
      // If button is pressed...
      if(pb1_stable == HIGH && pb1_last == LOW)
      {
        pb1_pressed_time = pb1_edge_time;
        pb1_short_pressed_time = pb1_long_pressed_time = 0;
        pb1_stable = pb1_last;
        pb1_pressed = true;
        pb1_short_pressed = false;
        pb1_long_pressed = false;
        pb1_released = false;
        pb1_short_released = false;
        pb1_long_released = false;
      }
      // If button is still pressed...
      else if(pb1_stable == LOW && pb1_last == LOW)
      {
        long pb1_press_duration = millis() - pb1_pressed_time;
        if(pb1_press_duration > SHORT_PRESS_TIME && (pb1_short_pressed_time - pb1_pressed_time) != SHORT_PRESS_TIME)
        {
          pb1_short_pressed = true;
          pb1_short_pressed_time = pb1_pressed_time + SHORT_PRESS_TIME;
        }
        if(pb1_press_duration > LONG_PRESS_TIME && (pb1_long_pressed_time - pb1_pressed_time) != LONG_PRESS_TIME)
        {
          pb1_short_pressed = false;
          pb1_long_pressed = true;
          pb1_long_pressed_time = pb1_pressed_time + LONG_PRESS_TIME;
        }
      }
      // If button is released...
      else if(pb1_stable == LOW && pb1_last == HIGH)
      {
        pb1_released_time = pb1_edge_time;
        pb1_stable = pb1_last;
        pb1_released = true;
        pb1_pressed = pb1_short_pressed = pb1_long_pressed = false;

        long pb1_press_duration = pb1_released_time - pb1_pressed_time;
        if(pb1_press_duration > LONG_PRESS_TIME)
        {
          pb1_short_released = false;
          pb1_long_released = true;
        }
        else if(pb1_press_duration > SHORT_PRESS_TIME)
        {
          pb1_short_released = true;
          pb1_long_released = false;
        }
      }
    }
  }
}

//
// Handle encoder PRESS + ROTATE
//
bool doPressAndRotate(int8_t dir)
{
  bool needRedraw = false;

  if(isSSB())
  {
#ifdef ENABLE_HOLDOFF
    // Tuning timer to hold off (FM/AM) display updates
    tuning_flag = true;
    tuning_timer = millis();
#endif
    updateBFO(currentBFO + dir * getSteps(true));
    needRedraw = true;
  }
  else
  {
    seekDirection = dir>0? 1 : 0;
    // G8PTN: Flag is set by rotary encoder and cleared on seek entry
    seekStop = false;
    doSeek();
    // G8PTN: Added to ensure update of currentFreq in table for AM/FM
    band[bandIdx].currentFreq = currentFrequency;
    needRedraw = true;
  }

  return(needRedraw);
}

//
// Handle encoder ROTATE
//
bool doRotate(int8_t dir)
{
  bool needRedraw = false;

  //
  // Side bar menus / settings
  //
  if(doSideBar(currentCmd, encoderCount))
  {
    // Side bar changed, need redraw
    needRedraw = true;
  }

  //
  // SSB tuning
  //
  else if(isSSB())
  {
#ifdef ENABLE_HOLDOFF
    // Tuning timer to hold off (SSB) display updates
    tuning_flag = true;
    tuning_timer = millis();
#endif
    updateBFO(currentBFO + dir * getSteps(false));
    needRedraw = true;
  }

  //
  // Normal tuning
  //
  else
  {
#ifdef ENABLE_HOLDOFF
    // Tuning timer to hold off (FM/AM) display updates
    tuning_flag = true;
    tuning_timer = millis();
#endif

    // G8PTN: Used in place of rx.frequencyUp() and rx.frequencyDown()
    uint16_t step = getCurrentStep()->step;
    uint16_t stepAdjust = currentFrequency % step;
    step = !stepAdjust? step : dir>0? step - stepAdjust : stepAdjust;

    // Tune to a new frequency
    updateFrequency(currentFrequency + step * dir);

    // Clear FM RDS information
    if(currentMode==FM) clearStationName();

    // Check current CB channel
    if(isCB()) checkCbChannel();

    // Will need a redraw
    needRedraw = true;
  }

  return(needRedraw);
}

//
// Main event loop
//
void loop()
{
  uint32_t currentTime = millis();
  bool needRedraw = false;

  // Block encoder rotation when display is off
  if(encoderCount && !displayOn()) encoderCount = 0;

  // If encoder has been rotated...
  if(encoderCount)
  {
    elapsedSleep = elapsedCommand = currentTime;

    // If encoder has been rotated AND pressed...
    if(pb1_pressed && !isModalMode(currentCmd))
    {
      needRedraw |= doPressAndRotate(encoderCount);
      seekModePress = true;
    }
    else
    {
      needRedraw |= doRotate(encoderCount);
    }

    // Clear encoder rotation
    encoderCount = 0;
    eepromRequestSave();
    needRedraw = true;
  }

  // Encoder released after LONG PRESS: TOGGLE DISPLAY
  else if(pb1_long_pressed && !seekModePress)
  {
    pb1_long_pressed = pb1_short_pressed = pb1_pressed = false;
    elapsedSleep = currentTime;
    needRedraw |= displayOn(!displayOn());
  }

  // Encoder released after SHORT PRESS: CHANGE VOLUME
  else if(pb1_short_released && displayOn() && !seekModePress)
  {
    pb1_released = pb1_short_released = pb1_long_released = false;
    elapsedSleep = elapsedCommand = currentTime;

    // Open volume control
    clickVolume();
    needRedraw = true;

    // Wait a little more for the button release
    delay(MIN_ELAPSED_TIME);
  }

  // ???: SELECT MENU ITEM
  else if(pb1_released && !pb1_long_released && !seekModePress)
  {
    pb1_released = pb1_short_released = pb1_long_released = false;
    elapsedSleep = elapsedCommand = currentTime;

    if(!displayOn())
    {
      if(currentSleep)
      {
        displayOn(true);
        needRedraw = true;
      }
    }
    else if(clickSideBar(currentCmd))
    {
      // Command handled, redraw screen
      needRedraw = true;
    }
    else if(isModalMode(currentCmd))
    {
      // Deactivate menu
      currentCmd = CMD_NONE;
      needRedraw = true;
    }
    else
    {
      // Activate menu
      currentCmd = CMD_MENU;
      needRedraw = true;
    }

    // Wait a little more for the button release
    delay(MIN_ELAPSED_TIME);
  }

  // Display sleep timeout
  if(currentSleep && displayOn() && ((currentTime - elapsedSleep) > currentSleep * 1000))
    displayOn(false);

  // Show RSSI status only if this condition has changed
  if((currentTime - elapsedRSSI) > MIN_ELAPSED_RSSI_TIME * 6)
  {
    rx.getCurrentReceivedSignalQuality();
    snr = rx.getCurrentSNR();

    // G8PTN: Based on 1.2s update, always allow S-Meter
    int newRssi = rx.getCurrentRSSI();
    if(newRssi != rssi)
    {
      rssi = newRssi;
      needRedraw = true;
    }

    elapsedRSSI = currentTime;
  }

  // Disable commands control
  if((currentTime - elapsedCommand) > ELAPSED_COMMAND)
  {
    if(isModalMode(currentCmd))
    {
      currentCmd = CMD_NONE;
      needRedraw = true;
    }

    elapsedCommand = currentTime;
  }

  if((currentTime - lastRDSCheck) > RDS_CHECK_TIME)
  {
    if((currentMode == FM) && (snr >= 12) && checkRds()) needRedraw = true;
    lastRDSCheck = currentTime;
  }

  // Tick EEPROM time, saving changes if the occurred and there has
  // been no activity for a while
  eepromTickTime();

  // Check for button activity
  buttonCheck();
  if(!pb1_pressed && seekModePress)
  {
    pb1_released = pb1_short_released = pb1_long_released = false;
    seekModePress = false;
  }

  // Periodically refresh the main screen
  // This covers the case where there is nothing else triggering a refresh
  if((currentTime - background_timer) > BACKGROUND_REFRESH_TIME)
  {
    if(!isModalMode(currentCmd)) needRedraw = true;
    background_timer = currentTime;
  }

#ifdef ENABLE_HOLDOFF
  // Check if tuning flag is set
  if(tuning_flag && ((currentTime - tuning_timer) > TUNE_HOLDOFF_TIME))
  {
    tuning_flag = false;
    needRedraw = true;
  }
#endif

  // Run clock
  needRedraw |= clockTickTime();

#ifndef DISABLE_REMOTE
  // Periodically print status to serial
  remoteTickTime();
  // Receive and execute serial command
  if(Serial.available()>0) needRedraw |= remoteDoCommand(Serial.read());
#endif

  // Redraw screen if necessary
  if(needRedraw) drawScreen();

  // Add a small default delay in the main loop
  delay(5);
}
