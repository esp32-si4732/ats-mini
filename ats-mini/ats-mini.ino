// =================================
// INCLUDE FILES
// =================================

#include "Common.h"
#include <Wire.h>
#include "EEPROM.h"
#include "Rotary.h"
#include "Button.h"
#include "Menu.h"
#include "Draw.h"
#include "Storage.h"
#include "Themes.h"
#include "Utils.h"
#include "EIBI.h"

// SI473/5 and UI
#define MIN_ELAPSED_TIME         5  // 300
#define MIN_ELAPSED_RSSI_TIME  200  // RSSI check uses IN_ELAPSED_RSSI_TIME * 6 = 1.2s
#define ELAPSED_COMMAND      10000  // time to turn off the last command controlled by encoder. Time to goes back to the VFO control // G8PTN: Increased time and corrected comment
#define DEFAULT_VOLUME          35  // change it for your favorite sound volume
#define DEFAULT_SLEEP            0  // Default sleep interval, range = 0 (off) to 255 in steps of 5
#define STRENGTH_CHECK_TIME   1500  // Not used
#define RDS_CHECK_TIME         250  // Increased from 90
#define SEEK_TIMEOUT        600000  // Max seek timeout (ms)
#define NTP_CHECK_TIME       60000  // NTP time refresh period (ms)
#define SCHEDULE_CHECK_TIME   2000  // How often to identify the same frequency (ms)
#define BACKGROUND_REFRESH_TIME 5000    // Background screen refresh time. Covers the situation where there are no other events causing a refresh
#define TUNE_HOLDOFF_TIME       90  // Timer to hold off display whilst tuning

// =================================
// CONSTANTS AND VARIABLES
// =================================

int8_t agcIdx = 0;
uint8_t disableAgc = 0;
int8_t agcNdx = 0;
int8_t softMuteMaxAttIdx = 4;

bool seekStop = false;        // G8PTN: Added flag to abort seeking on rotary encoder detection
bool pushAndRotate = false;   // Push and rotate is active, ignore the long press

long elapsedRSSI = millis();
long elapsedButton = millis();

long lastStrengthCheck = millis();
long lastRDSCheck = millis();
long lastNTPCheck = millis();
long lastScheduleCheck = millis();

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

// Menu options
uint8_t volume = DEFAULT_VOLUME;        // Volume, range = 0 (muted) - 63
uint8_t currentSquelch = 0;             // Squelch, range = 0 (disabled) - 127
bool squelchCutoff = false;             // True if the Squelch cutoff is in effect
uint8_t FmRegionIdx = 0;                // FM Region

uint16_t currentBrt = 130;              // Display brightness, range = 10 to 255 in steps of 5
uint16_t currentSleep = DEFAULT_SLEEP;  // Display sleep timeout, range = 0 to 255 in steps of 5
long elapsedSleep = millis();           // Display sleep timer
bool zoomMenu = false;                  // Display zoomed menu item
int8_t scrollDirection = 1;             // Menu scroll direction

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
ButtonTracker pb1 = ButtonTracker();
TFT_eSPI tft    = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);
SI4735_fixed rx;

//
// Hardware initialization and setup
//
void setup()
{
  // Enable serial port
  Serial.begin(115200);

  // Initialize flash file system
  diskInit();

  // Encoder pins. Enable internal pull-ups
  pinMode(ENCODER_PUSH_BUTTON, INPUT_PULLUP);
  pinMode(ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(ENCODER_PIN_B, INPUT_PULLUP);

  // Enable audio amplifier
  // Initally disable the audio amplifier until the SI4732 has been setup
  pinMode(PIN_AMP_EN, OUTPUT);
  digitalWrite(PIN_AMP_EN, LOW);

  // Enable SI4732 VDD
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);

  // The line below may be necessary to setup I2C pins on ESP32
  Wire.begin(ESP32_I2C_SDA, ESP32_I2C_SCL);

  // TFT display brightness control (PWM)
  // Note: At brightness levels below 100%, switching from the PWM may cause power spikes and/or RFI
  ledcAttach(PIN_LCD_BL, 16000, 8);  // Pin assignment, 16kHz, 8-bit
  ledcWrite(PIN_LCD_BL, 0);          // Default value 0%

  // TFT display setup
  tft.begin();
  tft.setRotation(3);

  // Detect and fix the mirrored & inverted display
  // https://github.com/esp32-si4732/ats-mini/issues/41
  if(tft.readcommand8(ST7789_RDDID, 3) == 0x93)
  {
    tft.invertDisplay(0);
    tft.writecommand(TFT_MADCTL);
    tft.writedata(TFT_MAD_MV | TFT_MAD_MX | TFT_MAD_MY | TFT_MAD_BGR);
  }

  tft.fillScreen(TH.bg);
  spr.createSprite(320, 170);
  spr.setTextDatum(MC_DATUM);
  spr.setSwapBytes(true);
  spr.setFreeFont(&Orbitron_Light_24);
  spr.setTextColor(TH.text, TH.bg);

  // Press and hold Encoder button to force an EEPROM reset
  // Note: EEPROM reset is recommended after firmware updates
  if(digitalRead(ENCODER_PUSH_BUTTON)==LOW)
  {
    netClearPreferences();
    eepromInvalidate();
    diskInit(true);

    ledcWrite(PIN_LCD_BL, 255);       // Default value 255 = 100%
    tft.setTextSize(2);
    tft.setTextColor(TH.text, TH.bg);
    tft.println(getVersion(true));
    tft.println();
    tft.setTextColor(TH.text_warn, TH.bg);
    tft.print("EEPROM Resetting");
    while(digitalRead(ENCODER_PUSH_BUTTON) == LOW) delay(100);
  }

  // Check for SI4732 connected on I2C interface
  // If the SI4732 is not detected, then halt with no further processing
  rx.setI2CFastModeCustom(100000);

  // Looks for the I2C bus address and set it.  Returns 0 if error
  int16_t si4735Addr = rx.getDeviceI2CAddress(RESET_PIN);
  if(!si4735Addr)
  {
    ledcWrite(PIN_LCD_BL, 255);       // Default value 255 = 100%
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
  }

  // ** SI4732 STARTUP **
  // Uses values from EEPROM (Last stored or defaults after EEPROM reset)
  selectBand(bandIdx, false);
  delay(50);
  rx.setVolume(volume);
  rx.setMaxSeekTime(SEEK_TIMEOUT);

  // Show help screen on first run
  if(eepromFirstRun())
  {
    // Clear screen buffer
    spr.fillSprite(TH.bg);
    ledcWrite(PIN_LCD_BL, currentBrt);
    drawAboutHelp(0);
    while(digitalRead(ENCODER_PUSH_BUTTON) != LOW) delay(100);
    while(digitalRead(ENCODER_PUSH_BUTTON) == LOW) delay(100);
  }

  // Draw display for the first time
  drawScreen();
  ledcWrite(PIN_LCD_BL, currentBrt);

  // Interrupt actions for Rotary encoder
  // Note: Moved to end of setup to avoid inital interrupt actions
  // ICACHE_RAM_ATTR void rotaryEncoder(); see rotaryEncoder implementation below.
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), rotaryEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), rotaryEncoder, CHANGE);

  // Connect WiFi, if necessary
  netInit(wifiModeIdx);

  // Start Bluetooth LE, if necessary
  bleInit(bleModeIdx);
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

  if(band->bandMode==FM)
  {
    // rx.setMaxDelaySetFrequency(60);
    rx.setFM(band->minimumFreq, band->maximumFreq, band->currentFreq, getCurrentStep()->step);
    // rx.setTuneFrequencyAntennaCapacitor(0);
    rx.setSeekFmLimits(band->minimumFreq, band->maximumFreq);

    // More sensitive seek thresholds
    // https://github.com/pu2clr/SI4735/issues/7#issuecomment-810963604
    rx.setSeekFmRssiThreshold(5); // default is 20
    rx.setSeekFmSNRThreshold(2); // default is 3

    rx.setFMDeEmphasis(fmRegions[FmRegionIdx].value);
    rx.RdsInit();
    rx.setRdsConfig(1, 2, 2, 2, 2);
    rx.setGpioCtl(1, 0, 0);   // G8PTN: Enable GPIO1 as output
    rx.setGpio(0, 0, 0);      // G8PTN: Set GPIO1 = 0
  }
  else
  {
    // rx.setMaxDelaySetFrequency(80);
    if(band->bandMode==AM)
    {
      rx.setAM(band->minimumFreq, band->maximumFreq, band->currentFreq, getCurrentStep()->step);
      // More sensitive seek thresholds
      // https://github.com/pu2clr/SI4735/issues/7#issuecomment-810963604
      rx.setSeekAmRssiThreshold(10); // default is 25
      rx.setSeekAmSNRThreshold(3); // default is 5
    }
    else
    {
      // Configure SI4732 for SSB (SI4732 step not used, set to 0)
      rx.setSSB(band->minimumFreq, band->maximumFreq, band->currentFreq, 0, currentMode);
      // G8PTN: Always enabled
      rx.setSSBAutomaticVolumeControl(1);
      // G8PTN: Commented out
      //rx.setSsbSoftMuteMaxAttenuation(softMuteMaxAttIdx);
      // To move frequency forward, need to move the BFO backwards
      if (currentMode == USB)
        rx.setSSBBfo(-(currentBFO + band->usbCal));
      else if (currentMode == LSB)
        rx.setSSBBfo(-(currentBFO + band->lsbCal));
      else
        rx.setSSBBfo(-currentBFO);  // No calibration if not USB/LSB
    }

    // Set the tuning capacitor for SW or MW/LW
    // rx.setTuneFrequencyAntennaCapacitor((band->bandType == MW_BAND_TYPE || band->bandType == LW_BAND_TYPE) ? 0 : 1);

    // G8PTN: Enable GPIO1 as output
    rx.setGpioCtl(1, 0, 0);
    // G8PTN: Set GPIO1 = 1
    rx.setGpio(1, 0, 0);
    // Consider the range all defined current band
    rx.setSeekAmLimits(band->minimumFreq, band->maximumFreq);
  }

  // Set step and spacing based on mode (FM, AM, SSB)
  doStep(0);
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
}

// This function is called by the seek function process.
bool checkStopSeeking()
{
  // Returns true if the user rotates the encoder
  if(seekStop) return true;

  // Checking isPressed without debouncing because this callback
  // is not invoked often enough to register a click
  if(pb1.update(digitalRead(ENCODER_PUSH_BUTTON) == LOW, 0).isPressed)
  {
    // Wait till the button is released, otherwise the main loop will register a click
    while(pb1.update(digitalRead(ENCODER_PUSH_BUTTON) == LOW).isPressed)
      delay(100);
    return true;
  }

  return false;
}

// This function is called by the seek function process.
void showFrequencySeek(uint16_t freq)
{
  currentFrequency = freq;
  drawScreen();
}

//
// Tune using BFO, using algorithm from Goshante's ATS-20_EX firmware
//
bool updateBFO(int newBFO, bool wrap)
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
    if(!wrap) return false;
    newFreq = band->maximumFreq;
    newBFO  = 0;
  }
  else if(f > band->maximumFreq * 1000)
  {
    if(!wrap) return false;
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
  if (currentMode == USB)
    rx.setSSBBfo(-(currentBFO + band->usbCal));
  else if (currentMode == LSB)
    rx.setSSBBfo(-(currentBFO + band->lsbCal));
  else
    rx.setSSBBfo(-currentBFO);  // No calibration if not USB/LSB

  // Save current band frequency, w.r.t. new BFO value
  band->currentFreq = currentFrequency + currentBFO / 1000;
  return true;
}

//
// Tune to a new frequency, resetting BFO if present
//
bool updateFrequency(int newFreq, bool wrap)
{
  Band *band = getCurrentBand();

  // Do not let new frequency exceed band limits
  if(newFreq < band->minimumFreq)
  {
    if(!wrap) return false; else newFreq = band->maximumFreq;
  }
  else if(newFreq > band->maximumFreq)
  {
    if(!wrap) return false; else newFreq = band->minimumFreq;
  }

  // Set new frequency
  rx.setFrequency(newFreq);

  // Clear BFO, if present
  if(currentBFO) updateBFO(0, true);

  // Update current frequency
  currentFrequency = rx.getFrequency();

  // Save current band frequency
  band->currentFreq = currentFrequency + currentBFO / 1000;
  return true;
}


//
// Handle encoder rotation in seek mode
//
bool doSeek(int8_t dir)
{
  // disable amp to avoid sound artifacts
  tempMuteOn(true);
  if(seekMode() == SEEK_DEFAULT)
  {
    if(isSSB())
    {
#ifdef ENABLE_HOLDOFF
      // Tuning timer to hold off (FM/AM) display updates
      tuning_flag = true;
      tuning_timer = millis();
#endif

      updateBFO(currentBFO + dir * getCurrentStep(true)->step, true);
    }
    else
    {
      // Clear stale parameters
      clearStationInfo();
      rssi = snr = 0;

      // G8PTN: Flag is set by rotary encoder and cleared on seek entry
      seekStop = false;
      rx.seekStationProgress(showFrequencySeek, checkStopSeeking, dir>0? 1 : 0);
      updateFrequency(rx.getFrequency(), true);
    }
  }
  else if(seekMode() == SEEK_SCHEDULE && dir)
  {
    uint8_t hour, minute;
    // Clock is valid because the above seekMode() call checks that
    clockGetHM(&hour, &minute);

    size_t offset = -1;
    const StationSchedule *schedule = dir > 0 ?
      eibiNext(currentFrequency + currentBFO / 1000, hour, minute, &offset) :
      eibiPrev(currentFrequency + currentBFO / 1000, hour, minute, &offset);

    if(schedule) updateFrequency(schedule->freq, false);
  }

  // Clear current station name and information
  clearStationInfo();
  // Check for named frequencies
  identifyFrequency(currentFrequency + currentBFO / 1000);
  // Will need a redraw
  // enable amp
  tempMuteOn(false);
  return(true);
}

//
// Handle tuning
//
bool doTune(int8_t dir)
{
  //
  // SSB tuning
  //
  if(isSSB())
  {
#ifdef ENABLE_HOLDOFF
    // Tuning timer to hold off (SSB) display updates
    tuning_flag = true;
    tuning_timer = millis();
#endif

    uint32_t step = getCurrentStep()->step;
    uint32_t stepAdjust = (currentFrequency * 1000 + currentBFO) % step;
    step = !stepAdjust? step : dir>0? step - stepAdjust : stepAdjust;

    updateBFO(currentBFO + dir * step, true);
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

    uint16_t step = getCurrentStep()->step;
    uint16_t stepAdjust = currentFrequency % step;
    stepAdjust = (currentMode==FM) && (step==20)? (stepAdjust+10) % step : stepAdjust;
    step = !stepAdjust? step : dir>0? step - stepAdjust : stepAdjust;

    // Tune to a new frequency
    updateFrequency(currentFrequency + step * dir, true);
  }

  // Clear current station name and information
  clearStationInfo();
  // Check for named frequencies
  identifyFrequency(currentFrequency + currentBFO / 1000);
  // Will need a redraw
  return(true);
}

//
// Rotate digit
//
bool doDigit(int8_t dir)
{
  bool updated = false;

  // SSB tuning
  if(isSSB())
  {
#ifdef ENABLE_HOLDOFF
    // Tuning timer to hold off (SSB) display updates
    tuning_flag = true;
    tuning_timer = millis();
#endif

    updated = updateBFO(currentBFO + dir * getFreqInputStep(), false);
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

    // Tune to a new frequency
    updated = updateFrequency(currentFrequency + getFreqInputStep() * dir, false);
  }

  if (updated) {
    // Clear current station name and information
    clearStationInfo();
    // Check for named frequencies
    identifyFrequency(currentFrequency + currentBFO / 1000);
  }

  // Will need a redraw
  return(updated);
}


bool clickFreq(bool shortPress)
{
  if (shortPress) {
    bool updated = false;

     // SSB tuning
     if(isSSB()) {
       updated = updateBFO(currentBFO - (currentFrequency * 1000 + currentBFO) % getFreqInputStep(), false);
     } else {
       // Normal tuning
       updated = updateFrequency(currentFrequency - currentFrequency % getFreqInputStep(), false);
     }

     if (updated) {
       // Clear current station name and information
       clearStationInfo();
       // Check for named frequencies
       identifyFrequency(currentFrequency + currentBFO / 1000);
     }
     return true;
  }
  return false;
}

bool processRssiSnr()
{
  static uint32_t updateCounter = 0;
  bool needRedraw = false;

  rx.getCurrentReceivedSignalQuality();
  int newRSSI = rx.getCurrentRSSI();
  int newSNR = rx.getCurrentSNR();

  // Apply squelch if the volume is not muted
  if(currentSquelch && currentSquelch <= 127)
  {
    if(newRSSI >= currentSquelch && squelchCutoff)
    {
      tempMuteOn(false);
      squelchCutoff = false;
    }
    else if(newRSSI < currentSquelch && !squelchCutoff)
    {
      tempMuteOn(true);
      squelchCutoff = true;
    }
  }
  else if(squelchCutoff)
  {
    tempMuteOn(false);
    squelchCutoff = false;
  }

  // G8PTN: Based on 1.2s interval, update RSSI & SNR
  if(!(updateCounter++ & 7))
  {
    // Show RSSI status only if this condition has changed
    if(newRSSI != rssi)
    {
      rssi = newRSSI;
      needRedraw = true;
    }
    // Show SNR status only if this condition has changed
    if(newSNR != snr)
    {
      snr = newSNR;
      needRedraw = true;
    }
  }
  return needRedraw;
}

//
// Main event loop
//
void loop()
{
  uint32_t currentTime = millis();
  bool needRedraw = false;

  ButtonTracker::State pb1st = pb1.update(digitalRead(ENCODER_PUSH_BUTTON) == LOW);

#ifndef DISABLE_REMOTE
  // Periodically print status to serial
  remoteTickTime();

  // Receive and execute serial command
  if(Serial.available()>0)
  {
    int revent = remoteDoCommand(Serial.read());
    needRedraw |= !!(revent & REMOTE_CHANGED);
    pb1st.wasClicked |= !!(revent & REMOTE_CLICK);
    int direction = revent >> REMOTE_DIRECTION;
    encoderCount = direction? direction : encoderCount;
    if(revent & REMOTE_EEPROM) eepromRequestSave();
  }
#endif

  int ble_event = bleDoCommand(bleModeIdx);

  // Block encoder rotation when in the locked sleep mode
  if(encoderCount && sleepOn() && sleepModeIdx==SLEEP_LOCKED) encoderCount = 0;

  // Activate push and rotate mode (can span multiple loop iterations until the button is released)
  if (encoderCount && pb1st.isPressed) pushAndRotate = true;

  // If push and rotate mode is active...
  if(pushAndRotate)
  {
    // If encoder has been rotated
    if(encoderCount)
    {
      switch(currentCmd)
      {
        case CMD_NONE:
          // Activate frequency input mode
          currentCmd = CMD_FREQ;
          needRedraw = true;
          break;
        case CMD_FREQ:
          // Select digit
          doSelectDigit(encoderCount);
          needRedraw = true;
          break;
        case CMD_SEEK:
          // Normal tuning in seek mode
          needRedraw |= doTune(encoderCount);
          eepromRequestSave();
          break;
      }

      // Clear encoder rotation
      encoderCount = 0;
    }
    // Reset timeouts while push and rotate is active
    elapsedSleep = elapsedCommand = currentTime;
  }
  else
  {
    // If encoder has been rotated
    if(encoderCount)
    {
      switch(currentCmd)
      {
        case CMD_NONE:
          // Tuning
          needRedraw |= doTune(encoderCount);
          break;
        case CMD_FREQ:
          // Digit tuning
          needRedraw |= doDigit(encoderCount);
          break;
        case CMD_SEEK:
          // Seek mode
          needRedraw |= doSeek(encoderCount);
          // Seek can take long time, renew the timestamp
          currentTime = millis();
          break;
        default:
          // Side bar menus / settings
          needRedraw |= doSideBar(currentCmd, encoderCount);
          break;
      }

      // Reset timeouts
      elapsedSleep = elapsedCommand = currentTime;
      eepromRequestSave();

      // Clear encoder rotation
      encoderCount = 0;
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
        if(currentSleep)
        {
          sleepOn(false);
          needRedraw = true;
        }
        else if(sleepModeIdx == SLEEP_UNLOCKED)
        {
          // Allow to adjust the volume in sleep mode
          if(pb1st.wasShortPressed && currentCmd==CMD_NONE)
            currentCmd = CMD_VOLUME;
          else if(currentCmd==CMD_VOLUME)
            clickHandler(currentCmd, pb1st.wasShortPressed);

          needRedraw = true;
        }
      }
      else if(clickHandler(currentCmd, pb1st.wasShortPressed))
      {
        // Command handled, redraw screen
        needRedraw = true;

        // EiBi can take long time, renew the timestamps
        elapsedSleep = elapsedCommand = currentTime = millis();
      }
      else if(currentCmd != CMD_NONE)
      {
        // Deactivate modal mode
        currentCmd = CMD_NONE;
        needRedraw = true;
      }
      else if(pb1st.wasShortPressed)
      {
        // Volume shortcut (only active in VFO mode)
        currentCmd = CMD_VOLUME;
        needRedraw = true;
      }
      else
      {
        // Activate menu
        currentCmd = CMD_MENU;
        needRedraw = true;
      }
    }
  }

  // Deactivate push and rotate mode
  if(!pb1st.isPressed && pushAndRotate)
  {
    pushAndRotate = false;
    needRedraw = true;
  }

  // Disable commands control
  if((currentTime - elapsedCommand) > ELAPSED_COMMAND)
  {
    if(currentCmd != CMD_NONE && currentCmd != CMD_SEEK)
    {
      currentCmd = CMD_NONE;
      needRedraw = true;
    }

    elapsedCommand = currentTime;
  }

  // Display sleep timeout
  if(currentSleep && !sleepOn() && ((currentTime - elapsedSleep) > currentSleep * 1000))
  {
    sleepOn(true);
    // CPU sleep can take long time, renew the timestamps
    elapsedSleep = elapsedCommand = currentTime = millis();
  }

  if((currentTime - elapsedRSSI) > MIN_ELAPSED_RSSI_TIME)
  {
    needRedraw |= processRssiSnr();
    elapsedRSSI = currentTime;
  }

  // Periodically check received RDS information
  if((currentTime - lastRDSCheck) > RDS_CHECK_TIME)
  {
    needRedraw |= (currentMode == FM) && (snr >= 12) && checkRds();
    lastRDSCheck = currentTime;
  }

  // Periodically check schedule
  if((currentTime - lastScheduleCheck) > SCHEDULE_CHECK_TIME)
  {
    needRedraw |= identifyFrequency(currentFrequency + currentBFO / 1000, true);
    lastScheduleCheck = currentTime;
  }

  // Periodically synchronize time via NTP
  if((currentTime - lastNTPCheck) > NTP_CHECK_TIME)
  {
    needRedraw |= ntpSyncTime();
    lastNTPCheck = currentTime;
  }

  // Tick EEPROM time, saving changes if the occurred and there has
  // been no activity for a while
  eepromTickTime();

  // Tick NETWORK time, connecting to WiFi if requested
  netTickTime();

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

  // Periodically refresh the main screen
  // This covers the case where there is nothing else triggering a refresh
  if(needRedraw) background_timer = currentTime;
  if((currentTime - background_timer) > BACKGROUND_REFRESH_TIME)
  {
    if(currentCmd == CMD_NONE) needRedraw = true;
    background_timer = currentTime;
  }

  // Redraw screen if necessary
  if(needRedraw) drawScreen();

  // Add a small default delay in the main loop
  delay(5);
}
