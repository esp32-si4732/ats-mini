// =================================
// INCLUDE FILES
// =================================

#include "Common.h"
#include <Wire.h>
#include "Rotary.h"
#include "Button.h"
#include "Menu.h"
#include "Draw.h"
#include "Storage.h"
#include "Themes.h"
#include "Utils.h"
#include "AudioManager.h"
#include "EIBI.h"
#include "Remote.h"
#include "BleMode.h"
#include "EventHandler.h"
#include "Scheduler.h"

RadioState radioState = {0};

// SI473/5 and UI
#define MIN_ELAPSED_TIME         5  // 300
#define ELAPSED_COMMAND      10000  // time to turn off the last command controlled by encoder. Time to goes back to the VFO control // G8PTN: Increased time and corrected comment
#define DEFAULT_VOLUME          35  // change it for your favorite sound volume
#define DEFAULT_SLEEP            0  // Default sleep interval, range = 0 (off) to 255 in steps of 5
#define SEEK_TIMEOUT        600000  // Max seek timeout (ms)

// =================================
// CONSTANTS AND VARIABLES
// =================================

volatile bool seekStop = false; // G8PTN: Added flag to abort seeking on rotary encoder detection

long elapsedRSSI = millis();
long elapsedButton = millis();

long lastStrengthCheck = millis();
long lastRDSCheck = millis();
long lastNTPCheck = millis();
long lastScheduleCheck = millis();

long elapsedCommand = millis();
volatile int16_t encoderCount = 0;
volatile int16_t encoderCountAccel = 0;

long elapsedSleep = millis();           // Display sleep timer

// Signal quality (kept as standalone variables -- too short for safe global macros)
uint8_t rssi = 0;
uint8_t snr = 0;

// Background screen refresh
uint32_t background_timer = millis();   // Background screen refresh timer.

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

  // Encoder pins. Enable internal pull-ups
  pinMode(ENCODER_PUSH_BUTTON, INPUT_PULLUP);
  pinMode(ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(ENCODER_PIN_B, INPUT_PULLUP);

  // Initially disable the audio amplifier until the SI4732 has been setup,
  // if the target board exposes a separate amplifier enable pin.
  if(PIN_AMP_EN >= 0)
  {
    pinMode(PIN_AMP_EN, OUTPUT);
    digitalWrite(PIN_AMP_EN, LOW);
  }

  // Enable SI4732 VDD
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);
  delay(100);

  // The line below may be necessary to setup I2C pins on ESP32
  Wire.begin(ESP32_I2C_SDA, ESP32_I2C_SCL);

  // TFT display brightness control (PWM)
  // Note: At brightness levels below 100%, switching from the PWM may cause power spikes and/or RFI
  ledcAttach(PIN_LCD_BL, 16000, 8);  // Pin assignment, 16kHz, 8-bit
  ledcWrite(PIN_LCD_BL, 0);          // Default value 0%

  // TFT display setup
  tft.begin();
  tft.setRotation(3);

  #if !defined(LILYGO_SI473X)
  // Detect and fix the mirrored & inverted display
  // https://github.com/esp32-si4732/ats-mini/issues/41
  uint8_t did3 = tft.readcommand8(ST7789_RDDID, 3);
  // 0x048181B3 - the original display
  // 0x04858552 - high gamma display
  // 0x00009307 - inverted & mirrored display
  if(did3 == 0x93)
  {
    tft.invertDisplay(0);
    tft.writecommand(TFT_MADCTL);
    tft.writedata(TFT_MAD_MV | TFT_MAD_MX | TFT_MAD_MY | TFT_MAD_BGR);
  }
  else if(did3 == 0x85)
  {
    tft.writecommand(0x26); // GAMSET
    tft.writedata(8);       // Gamma Curve 3

    tft.writecommand(0x55); // WRCACE (content adaptive brightness and color)
    tft.writedata(0xB1);    // High enhancement, UI mode
  }
  #endif

  tft.fillScreen(TH.bg);
  spr.createSprite(320, 170);
  spr.setTextDatum(MC_DATUM);
  spr.setSwapBytes(true);
  spr.setFreeFont(&Orbitron_Light_24);
  spr.setTextColor(TH.text, TH.bg);

  // Press and hold Encoder button to force an preferences reset
  // Note: preferences reset is recommended after firmware updates
  if(digitalRead(ENCODER_PUSH_BUTTON)==LOW)
  {
    nvsErase();
    diskInit(true);

    ledcWrite(PIN_LCD_BL, 255);       // Default value 255 = 100%
    tft.setTextSize(2);
    tft.setTextColor(TH.text, TH.bg);
    tft.println(getVersion(true));
    tft.println();
    tft.setTextColor(TH.text_warn, TH.bg);
    tft.print("Resetting Preferences");
    while(digitalRead(ENCODER_PUSH_BUTTON) == LOW) delay(100);
  }

  // Initialize flash file system
  diskInit();

  if(!ESP.getPsramSize()) {
    ledcWrite(PIN_LCD_BL, 255);       // Default value 255 = 100%
    tft.setTextSize(2);
    tft.setTextColor(TH.text_warn, TH.bg);
    tft.println("PSRAM not detected");
#ifdef CONFIG_SPIRAM_MODE_OCT
    tft.println("(try the QSPI f/w version)");
#else
    tft.println("(try the OSPI f/w version)");
#endif
  while(1);
  }

  // Check for SI4732 connected on I2C interface
  // If the SI4732 is not detected, then halt with no further processing
  rx.setI2CFastModeCustom(800000UL);

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

  // Set defaults for prefsLoad() fallbacks
  radioState.vol              = DEFAULT_VOLUME;
  radioState.brightness      = 130;
  radioState.amAvcIdx        = 48;
  radioState.ssbAvcIdx       = 48;
  radioState.amSoftMuteIdx   = 4;
  radioState.ssbSoftMuteIdx  = 4;
  radioState.softMuteMaxAtt  = 4;
  radioState.scrollDir       = 1;

  // If loading preferences fails...
  if(!prefsLoad(SAVE_SETTINGS|SAVE_VERIFY))
  {
    // Save default preferences
    prefsSave(SAVE_SETTINGS);
    // Show initial screen with the QR code
    spr.fillSprite(TH.bg);
    ledcWrite(PIN_LCD_BL, radioState.brightness);
    drawAboutHelp(0);
    // Wait for an encoder click
    while(digitalRead(ENCODER_PUSH_BUTTON)!=LOW) delay(100);
    while(digitalRead(ENCODER_PUSH_BUTTON)==LOW) delay(100);
  }

  // If loading memories fails, save default memories
  if(!prefsLoad(SAVE_MEMORIES|SAVE_VERIFY)) prefsSave(SAVE_MEMORIES);

  // If loading bands fails, save default bands
  if(!prefsLoad(SAVE_BANDS|SAVE_VERIFY)) prefsSave(SAVE_BANDS);

  // Audio Amplifier Enable. G8PTN: Added
  // After the SI4732 has been setup, enable the audio amplifier
  if(PIN_AMP_EN >= 0) digitalWrite(PIN_AMP_EN, HIGH);

  // SI4732 STARTUP!
  selectBand(bandIdx, false);
  delay(50);
  rx.setVolume(radioState.vol);
  rx.setMaxSeekTime(SEEK_TIMEOUT);

  // Draw display for the first time
  drawScreen();
  ledcWrite(PIN_LCD_BL, radioState.brightness);

  // Interrupt actions for Rotary encoder
  // Note: Moved to end of setup to avoid inital interrupt actions
  // ICACHE_RAM_ATTR void rotaryEncoder(); see rotaryEncoder implementation below.
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), rotaryEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), rotaryEncoder, CHANGE);

  // Connect WiFi, if necessary
  netInit(radioState.wifiMode);

  // Start Bluetooth LE, if necessary
  bleInit(radioState.bleMode);
}


int16_t accelerateEncoder(int8_t dir)
{
  const uint32_t speedThresholds[] = {350, 60, 45, 35, 25}; // ms between clicks
  const uint16_t accelFactors[] =      {1,  2,  4,  8, 16}; // corresponding multipliers
  static uint32_t lastEncoderTime = 0;
  static uint32_t lastSpeed = speedThresholds[0];
  static uint16_t lastAccelFactor = accelFactors[0];
  static int8_t lastEncoderDir = 0;

  uint32_t currentTime = millis();
  lastSpeed = ((currentTime - lastEncoderTime) * 7 + lastSpeed * 3) / 10;

  // Reset acceleration on timeout or direction change
  if (lastSpeed > speedThresholds[0] || lastEncoderDir != dir) {
    lastSpeed = speedThresholds[0];
    lastAccelFactor = accelFactors[0];
  } else {
    // Lookup acceleration factor
    for (int8_t i = LAST_ITEM(speedThresholds); i >= 0; i--) {
      if (lastSpeed <= speedThresholds[i] && lastAccelFactor < accelFactors[i]) {
        lastAccelFactor = accelFactors[i];
        break;
      }
    }
  }
  lastEncoderTime = currentTime;
  lastEncoderDir = dir;

  // Apply acceleration with direction
  return(dir * lastAccelFactor);
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
    int8_t delta = encoderStatus==DIR_CW? 1 : -1;
    int16_t accelDelta = accelerateEncoder(delta);

    // Do not accumulate too many encoder steps if event loop doesn't consume them
    if(abs(encoderCount) < 5)
    {
      encoderCount += delta;
      encoderCountAccel += accelDelta;
    }

    // Reset the seek flag
    seekStop = true;
  }
}

//
// Switch radio to given band
//
void useBand(const Band *band)
{
  // Set current frequency and mode, reset BFO
  radioState.frequency = band->currentFreq;
  radioState.mode = band->bandMode;
  radioState.bfo = 0;

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

    rx.setFMDeEmphasis(fmRegions[radioState.fmRegionIdx].value);
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
      rx.setSSB(band->minimumFreq, band->maximumFreq, band->currentFreq, 0, radioState.mode);
      // G8PTN: Always enabled
      rx.setSSBAutomaticVolumeControl(1);
      // G8PTN: Commented out
      //rx.setSsbSoftMuteMaxAttenuation(radioState.softMuteMaxAtt);
      // To move frequency forward, need to move the BFO backwards
      if (radioState.mode == USB)
        rx.setSSBBfo(-(radioState.bfo + band->usbCal));
      else if (radioState.mode == LSB)
        rx.setSSBBfo(-(radioState.bfo + band->lsbCal));
      else
        rx.setSSBBfo(-radioState.bfo);  // No calibration if not USB/LSB
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
  // Set softMuteMaxAtt based on mode (AM, SSB)
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

//
// Tune using BFO, using algorithm from Goshante's ATS-20_EX firmware
//
bool updateBFO(int newBFO, bool wrap)
{
  Band *band = getCurrentBand();
  int newFreq = radioState.frequency;

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
  if(newFreq != radioState.frequency)
  {
    // Apply new frequency
    rx.setFrequency(newFreq);

    // Re-apply to remove noise
    doAgc(0);
    // Update current frequency
    radioState.frequency = rx.getFrequency();
  }

  // Update current BFO
  radioState.bfo = newBFO;

  // To move frequency forward, need to move the BFO backwards
  if (radioState.mode == USB)
    rx.setSSBBfo(-(radioState.bfo + band->usbCal));
  else if (radioState.mode == LSB)
    rx.setSSBBfo(-(radioState.bfo + band->lsbCal));
  else
    rx.setSSBBfo(-radioState.bfo);  // No calibration if not USB/LSB

  // Save current band frequency, w.r.t. new BFO value
  band->currentFreq = radioState.frequency + radioState.bfo / 1000;
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
  if(radioState.bfo) updateBFO(0, true);

  // Update current frequency
  radioState.frequency = rx.getFrequency();

  // Save current band frequency
  band->currentFreq = radioState.frequency + radioState.bfo / 1000;
  return true;
}

// This function is called by blocking operations that need a lightweight abort check.
bool consumeAbortPending()
{
  if(seekStop)
  {
    seekStop = false;
    return true;
  }
  if(bleConsumeAbortPending(radioState.bleMode)) return true;
  if(serialConsumeAbortPending(radioState.usbMode)) return true;

  // Checking isPressed without debouncing because this helper is used from
  // blocking operations that do not run the normal event loop often enough.
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
  radioState.frequency = freq;
  drawScreen();
}

//
// Handle encoder rotation in seek mode
//
bool doSeek(int16_t enc, int16_t enca)
{
  // disable amp to avoid sound artifacts
  audioTempMute(true);
  if(seekMode() == SEEK_DEFAULT)
  {
    if(isSSB())
    {
      updateBFO(radioState.bfo + enca * getCurrentStep()->step, true);
    }
    else
    {
      // Clear stale parameters
      clearStationInfo();
      rssi = snr = 0;

      // Clear stale abort state before starting seek
      consumeAbortPending();
      rx.seekStationProgress(showFrequencySeek, consumeAbortPending, enc>0? 1 : 0);
      updateFrequency(rx.getFrequency(), true);
    }
  }
  else if(seekMode() == SEEK_SCHEDULE && enc)
  {
    uint8_t hour, minute;
    // Clock is valid because the above seekMode() call checks that
    clockGetHM(&hour, &minute);

    size_t offset = -1;
    const StationSchedule *schedule = enc > 0 ?
      eibiNext(radioState.frequency + radioState.bfo / 1000, hour, minute, &offset) :
      eibiPrev(radioState.frequency + radioState.bfo / 1000, hour, minute, &offset);

    if(schedule) updateFrequency(schedule->freq, false);
  }

  // Clear current station name and information
  clearStationInfo();
  // Check for named frequencies
  identifyFrequency(radioState.frequency + radioState.bfo / 1000);
  // Will need a redraw
  // enable amp
  audioTempMute(false);
  return(true);
}

//
// Handle tuning
//
bool doTune(int16_t enc)
{
  //
  // SSB tuning
  //
  if(isSSB())
  {
    uint32_t step = getCurrentStep()->step;
    uint32_t stepAdjust = (radioState.frequency * 1000 + radioState.bfo) % step;
    step = !stepAdjust? step : enc>0? step - stepAdjust : stepAdjust;

    updateBFO(radioState.bfo + enc * step, true);
  }

  //
  // Normal tuning
  //
  else
  {
    uint16_t step = getCurrentStep()->step;
    uint16_t stepAdjust = radioState.frequency % step;
    stepAdjust = (radioState.mode==FM) && (step==20)? (stepAdjust+10) % step : stepAdjust;
    step = !stepAdjust? step : enc>0? step - stepAdjust : stepAdjust;

    // Tune to a new frequency
    updateFrequency(radioState.frequency + step * enc, true);
  }

  // Clear current station name and information
  clearStationInfo();
  // Check for named frequencies
  identifyFrequency(radioState.frequency + radioState.bfo / 1000);
  // Will need a redraw
  return(true);
}

//
// Rotate digit
//
bool doDigit(int16_t enc)
{
  bool updated = false;

  // SSB tuning
  if(isSSB())
  {
    updated = updateBFO(radioState.bfo + enc * getFreqInputStep(), false);
  }

  //
  // Normal tuning
  //
  else
  {
    // Tune to a new frequency
    updated = updateFrequency(radioState.frequency + enc * getFreqInputStep(), false);
  }

  if (updated) {
    // Clear current station name and information
    clearStationInfo();
    // Check for named frequencies
    identifyFrequency(radioState.frequency + radioState.bfo / 1000);
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
       updated = updateBFO(radioState.bfo - (radioState.frequency * 1000 + radioState.bfo) % getFreqInputStep(), false);
     } else {
       // Normal tuning
       updated = updateFrequency(radioState.frequency - radioState.frequency % getFreqInputStep(), false);
     }

     if (updated) {
       // Clear current station name and information
       clearStationInfo();
       // Check for named frequencies
       identifyFrequency(radioState.frequency + radioState.bfo / 1000);
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
  uint8_t squelchValue = radioState.squelch[radioState.mode] & 0x7f;
  uint8_t squelchParam = (radioState.squelch[radioState.mode] & 0x80)? newSNR:newRSSI;
  if(squelchValue)
  {
    if(squelchParam >= squelchValue && audioIsSquelched())
    {
      audioSquelchClose(false);
    }
    else if(squelchParam < squelchValue && !audioIsSquelched())
    {
      audioSquelchClose(true);
    }
  }
  else if(audioIsSquelched())
  {
    audioSquelchClose(false);
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

  // Handle encoder, button, serial, and BLE input
  needRedraw |= handleEncoderInput();

  // Re-capture timestamp after input processing (handleEncoderInput may have updated elapsedCommand)
  currentTime = millis();

  // Disable commands control
  if((currentTime - elapsedCommand) > ELAPSED_COMMAND)
  {
    // if(getCpuFrequencyMhz()!=80) setCpuFrequencyMhz(80);
    if(radioState.cmd != CMD_NONE && radioState.cmd != CMD_SEEK && radioState.cmd != CMD_SCAN && radioState.cmd != CMD_MEMORY)
    {
      radioState.cmd = CMD_NONE;
      needRedraw = true;
    }

    elapsedCommand = currentTime;
  }

  // Display sleep timeout
  if(radioState.sleep && !sleepOn() && ((currentTime - elapsedSleep) > radioState.sleep * 1000))
  {
    sleepOn(true);
    // CPU sleep can take long time, renew the timestamps
    elapsedSleep = elapsedCommand = currentTime = millis();
  }

  // Run periodic housekeeping tasks
  needRedraw |= runScheduler(currentTime);

  // Redraw screen if necessary
  if(needRedraw) drawScreen();

  // Add a small default delay in the main loop
  delay(5);
}
