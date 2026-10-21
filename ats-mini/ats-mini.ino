// =================================
// INCLUDE FILES
// =================================

#include "Common.h"
#include <Wire.h>
#include "Rotary.h"
#include "Button.h"
#include "Menu.h"
#include "Memories.h"
#include "Patches.h"
#include "Draw.h"
#include "Storage.h"
#include "Themes.h"
#include "Utils.h"
#include "EIBI.h"
#include "Remote.h"
#include "BleMode.h"
#include "TcpMode.h"
#include "Splash.h"
#include "Web.h"
#include <stdlib.h>
#include <time.h>

// SI473/5 and UI
#define MIN_ELAPSED_TIME         5  // 300
#define MIN_ELAPSED_RSSI_TIME  200  // RSSI check uses IN_ELAPSED_RSSI_TIME * 6 = 1.2s
#define ELAPSED_COMMAND      10000  // time to turn off the last command controlled by encoder. Time to goes back to the VFO control // G8PTN: Increased time and corrected comment
#define DEFAULT_VOLUME          35  // change it for your favorite sound volume
#define DEFAULT_SLEEP            0  // Default sleep interval, range = 0 (off) to 255 in steps of 5
#define RDS_CHECK_TIME         250  // Increased from 90
#define SEEK_TIMEOUT        600000  // Max seek timeout (ms)
#define SCHEDULE_CHECK_TIME   2000  // How often to identify the same frequency (ms)

// =================================
// CONSTANTS AND VARIABLES
// =================================

int8_t agcIdx = 0;
uint8_t disableAgc = 0;
int8_t agcNdx = 0;
int8_t softMuteMaxAttIdx = 4;

volatile bool seekStop = false; // G8PTN: Added flag to abort seeking on rotary encoder detection
bool pushAndRotate = false;   // Combined physical/BLE gesture state for the display

long elapsedRSSI = millis();
long elapsedButton = millis();

long lastStrengthCheck = millis();
long lastRDSCheck = millis();
long lastScheduleCheck = millis();

long elapsedCommand = millis();
volatile int16_t encoderCount = 0;
volatile int16_t encoderCountAccel = 0;
uint32_t currentFrequency;

// AGC/ATTN index per mode (FM/AM/SSB)
int8_t FmAgcIdx = 0;                    // Default FM  AGGON  : Range = 0 to 37, 0 = AGCON, 1 - 27 = ATTN 0 to 26
int8_t AmAgcIdx = 0;                    // Default AM  AGCON  : Range = 0 to 37, 0 = AGCON, 1 - 37 = ATTN 0 to 36
int8_t SsbAgcIdx = 0;                   // Default SSB AGCON  : Range = 0 to 1,  0 = AGCON,      1 = ATTN 0

// AVC index per mode (AM/SSB)
int8_t AmAvcIdx = 48;                   // Default AM  = 48 (as per AN332), range = 12 to 90 in steps of 2
int8_t SsbAvcIdx = 48;                  // Default SSB = 48, range = 12 to 90 in steps of 2
bool ssbAvcHold = false;                // Temporary; cleared when the band/mode is reinitialized

// SoftMute index per mode (AM/SSB)
int8_t AmSoftMuteIdx = 4;               // Default AM  = 4, range = 0 to 32
int8_t SsbSoftMuteIdx = 4;              // Default SSB = 4, range = 0 to 32

// Menu options
uint8_t volume = DEFAULT_VOLUME;        // Volume, range = 0 (muted) - 63
uint8_t currentSquelch[4] = {0};        // Squelch per mode: lower 7 bits = threshold, high bit selects SNR (1) vs RSSI (0)
uint8_t FmRegionIdx = 0;                // FM Region

uint16_t currentBrt = 130;              // Display brightness, range = 10 to 255 in steps of 5
uint16_t currentSleep = DEFAULT_SLEEP;  // Display sleep timeout, range = 0 to 255 in steps of 5
long elapsedSleep = millis();           // Display sleep timer
bool zoomMenu = false;                  // Display zoomed menu item
int8_t scrollDirection = 1;             // Menu scroll direction
#if defined(LILYGO_SI473X)
bool encoderHalfStep = true;             // T-Embed defaults to half-step decoding
#else
bool encoderHalfStep = false;            // Standard hardware defaults to full-step decoding
#endif

// Background screen refresh
uint32_t background_timer = millis();   // Background screen refresh timer.

//
// Current parameters
//
uint16_t currentCmd  = CMD_NONE;
uint8_t  currentMode = FM;

uint8_t  rssi = 0;
uint8_t  snr  = 0;

//
// Devices
//
Rotary encoder  = Rotary(ENCODER_PIN_B, ENCODER_PIN_A, encoderHalfStep);
ButtonTracker pb1 = ButtonTracker();
LGFX tft;
LGFX_Sprite spr(&tft);
SI4735_fixed rx;

//
// Hardware initialization and setup
//
void setup()
{
  // Keep the C library clock in UTC; display offsets are applied separately.
  setenv("TZ", "UTC0", 1);
  tzset();

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

  // Firmware owns the mute pin so library power-up cannot release it
  pinMode(AUDIO_MUTE, OUTPUT);
  digitalWrite(AUDIO_MUTE, LOW); // Keep inactive until the hardware setting is loaded.

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
  tft.init();
  tft.setRotation(3);

  tft.fillScreen(TH.bg);
  spr.setPsram(true);
  spr.setColorDepth(16);
  spr.createSprite(DISPLAY_WIDTH, DISPLAY_HEIGHT);
  spr.setTextDatum(MC_DATUM);
  spr.setFont(&lgfx::fonts::Orbitron_Light_24);
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

  if(!ESP.getPsramSize() || !memoriesInit()) {
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

  // If loading preferences fails...
  if(!prefsLoad(SAVE_SETTINGS|SAVE_VERIFY))
  {
    // Save default preferences
    prefsSave(SAVE_SETTINGS);
    // Show initial screen with the QR code
    spr.fillSprite(TH.bg);
    ledcWrite(PIN_LCD_BL, currentBrt);
    drawAboutHelp(0);
    // Wait for an encoder click
    while(digitalRead(ENCODER_PUSH_BUTTON)!=LOW) delay(100);
    while(digitalRead(ENCODER_PUSH_BUTTON)==LOW) delay(100);
  }

  digitalWrite(AUDIO_MUTE, audioClamp ? HIGH : LOW);

  // Start a fresh countdown using the configured duration.
  offTimerRestart();

  // If loading memories fails, save default memories
  if(!prefsLoad(SAVE_MEMORIES|SAVE_VERIFY)) prefsSave(SAVE_MEMORIES);

  // If loading bands fails, save default bands
  if(!prefsLoad(SAVE_BANDS|SAVE_VERIFY)) prefsSave(SAVE_BANDS);

  // Audio Amplifier Enable. G8PTN: Added
  // After the SI4732 has been setup, enable the audio amplifier
  if(PIN_AMP_EN >= 0) digitalWrite(PIN_AMP_EN, HIGH);

  patchesInit();

  // SI4732 STARTUP!
  initVfo();
  delay(50);
  rx.setVolume(volume);
  rx.setMaxSeekTime(SEEK_TIMEOUT);

  // Draw display for the first time
  if(splashDraw())
  {
    ledcWrite(PIN_LCD_BL, currentBrt);
    delay(3000);
  }
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

void setEncoderHalfStep(bool enabled)
{
  noInterrupts();
  encoder.setHalfStep(enabled);
  encoderHalfStep = enabled;
  encoderCount = 0;
  encoderCountAccel = 0;
  interrupts();
}

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

// Select the nearest point on the 28 kHz SSB tuning grid within the band.
// Return receiver units: 10 kHz for FM, 1 kHz for AM/SSB.
uint16_t getReceiverFrequency(uint32_t freq)
{
  if(isSSB())
  {
    const Band *band = getCurrentBand();
    const uint32_t spacing = 2 * MAX_BFO;
    freq = ((freq + MAX_BFO) / spacing) * spacing;
    freq = constrain(freq, band->minimumFreq, band->maximumFreq);
  }
  return freqFromHz(freq, currentMode);
}

int getCurrentBFO()
{
  return isSSB()? (int32_t)currentFrequency - (int32_t)freqToHz(getReceiverFrequency(currentFrequency), currentMode) : 0;
}

static void applyBFO()
{
  const Band *band = getCurrentBand();
  int calibration = currentMode == USB? band->usbCal : currentMode == LSB? band->lsbCal : 0;
  rx.setSSBBfo(-(getCurrentBFO() + calibration));
}

//
// Switch radio to given band
//
void useBand(Band *band)
{
  // Convert the saved Hz frequency to receiver units and restore SSB fine tuning.
  currentMode = band->bandMode;
  uint16_t receiverFreq = getReceiverFrequency(band->currentFreq);
  currentFrequency = isSSB()? band->currentFreq : freqToHz(receiverFreq, currentMode);
  // AM/FM discard fractions smaller than a receiver unit.
  band->currentFreq = currentFrequency;
  uint16_t minFreq = freqFromHz(band->minimumFreq, currentMode);
  uint16_t maxFreq = freqFromHz(band->maximumFreq, currentMode);
  ssbAvcHold = false;

  if(band->bandMode==FM)
  {
    // rx.setMaxDelaySetFrequency(60);
    rx.setFM(minFreq, maxFreq, receiverFreq, freqFromHz(getCurrentStep()->step, currentMode));
    // rx.setTuneFrequencyAntennaCapacitor(0);
    rx.setSeekFmLimits(minFreq, maxFreq);

    // More sensitive seek thresholds
    // https://github.com/pu2clr/SI4735/issues/7#issuecomment-810963604
    rx.setSeekFmRssiThreshold(5); // default is 20
    rx.setSeekFmSNRThreshold(2); // default is 3

    rx.setFMDeEmphasis(fmRegions[FmRegionIdx].value);
    applyFmStereo();
    rx.RdsInit();
    rx.setRdsConfig(getRDSMode()!=RDS_OFF, 2, 2, 2, 2);
    rx.setGpioCtl(1, 0, 0);   // G8PTN: Enable GPIO1 as output
    rx.setGpio(0, 0, 0);      // G8PTN: Set GPIO1 = 0
  }
  else
  {
    // rx.setMaxDelaySetFrequency(80);
    if(band->bandMode==AM)
    {
      rx.setAM(minFreq, maxFreq, receiverFreq, freqFromHz(getCurrentStep()->step, currentMode));
      // More sensitive seek thresholds
      // https://github.com/pu2clr/SI4735/issues/7#issuecomment-810963604
      rx.setSeekAmRssiThreshold(10); // default is 25
      rx.setSeekAmSNRThreshold(3); // default is 5
    }
    else
    {
      // Configure SI4732 for SSB (SI4732 step not used, set to 0)
      rx.setSSB(minFreq, maxFreq, receiverFreq, 0, currentMode);
      // Initialize SSB with automatic AVC and AFC disabled
      rx.setSSBConfig(getCurrentBandwidth()->idx, 1, 0, 1, 0, 1);
      // G8PTN: Commented out
      //rx.setSsbSoftMuteMaxAttenuation(softMuteMaxAttIdx);
      applyBFO();
    }

    // Set the tuning capacitor for SW or MW/LW
    // rx.setTuneFrequencyAntennaCapacitor((band->bandType == MW_BAND_TYPE || band->bandType == LW_BAND_TYPE) ? 0 : 1);

    // G8PTN: Enable GPIO1 as output
    rx.setGpioCtl(1, 0, 0);
    // G8PTN: Set GPIO1 = 1
    rx.setGpio(1, 0, 0);
    // Consider the range all defined current band
    rx.setSeekAmLimits(minFreq, maxFreq);
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

//
// Tune to a frequency in Hz. SSB uses a fixed tuning grid and a derived BFO.
//
bool updateFrequency(int64_t newFreq, bool wrap)
{
  Band *band = getCurrentBand();
  if(newFreq < band->minimumFreq)
  {
    if(!wrap) return false; else newFreq = band->maximumFreq;
  }
  else if(newFreq > band->maximumFreq)
  {
    if(!wrap) return false; else newFreq = band->minimumFreq;
  }

  uint16_t receiverFreq = getReceiverFrequency(newFreq);
  if(!isSSB() || receiverFreq != getReceiverFrequency(currentFrequency))
  {
    rx.setFrequency(receiverFreq);
    if(isSSB()) doAgc(0);
  }

  currentFrequency = isSSB()? newFreq : freqToHz(rx.getFrequency(), currentMode);
  if(isSSB()) applyBFO();
  band->currentFreq = currentFrequency;
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
  if(bleConsumeAbortPending(bleModeIdx)) return true;
  if(serialConsumeAbortPending(usbModeIdx)) return true;
  if(tcpConsumeAbortPending(tcpModeIdx)) return true;

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
  currentFrequency = freqToHz(freq, currentMode);
  drawScreen();
}

//
// Handle encoder rotation in seek mode
//
bool doSeek(int16_t enc, int16_t enca)
{
  // Silence seek audio using the configured hardware mute method.
  muteOn(MUTE_TEMP, true, !audioClamp);
  if(seekMode() == SEEK_DEFAULT)
  {
    if(isSSB())
    {
      updateFrequency(currentFrequency + (int64_t)enca * getCurrentStep()->step, true);
    }
    else
    {
      // Clear stale parameters
      clearStationInfo();
      rssi = snr = 0;

      // Clear only the encoder latch. Pending remote input cancels the seek
      // and is consumed without executing its command.
      seekStop = false;
      if(!consumeAbortPending())
        rx.seekStationProgress(showFrequencySeek, consumeAbortPending, enc>0? 1 : 0);
      updateFrequency(freqToHz(rx.getFrequency(), currentMode), true);
    }
  }
  else if(seekMode() == SEEK_SCHEDULE && enc)
  {
    uint8_t hour, minute;
    // Clock is valid because the above seekMode() call checks that
    clockGetHM(&hour, &minute);

    size_t offset = -1;
    const StationSchedule *schedule = enc > 0 ?
      eibiNext(freqFromHz(currentFrequency, currentMode), hour, minute, &offset) :
      eibiPrev(freqFromHz(currentFrequency, currentMode), hour, minute, &offset);

    if(schedule) updateFrequency(schedule->freq * 1000, false);
  }

  // Clear current station name and information
  clearStationInfo();
  // Check for named frequencies
  identifyFrequency(currentFrequency);
  // Will need a redraw
  muteOn(MUTE_TEMP, false, !audioClamp);
  return(true);
}

//
// Handle tuning
//
bool doTune(int16_t enc)
{
  int32_t step = getCurrentStep()->step;
  int32_t stepAdjust = currentFrequency % step;
  if(currentMode == FM && step == 200000) stepAdjust = (stepAdjust + 100000) % step;
  step = !stepAdjust? step : enc>0? step - stepAdjust : stepAdjust;
  updateFrequency(currentFrequency + (int64_t)step * enc, true);

  // Clear current station name and information
  clearStationInfo();
  // Check for named frequencies
  identifyFrequency(currentFrequency);
  // Will need a redraw
  return(true);
}

//
// Rotate digit
//
bool doDigit(int16_t enc)
{
  bool updated = updateFrequency(currentFrequency + (int64_t)enc * getFreqInputStep(), false);

  if (updated) {
    // Clear current station name and information
    clearStationInfo();
    // Check for named frequencies
    identifyFrequency(currentFrequency);
  }

  // Will need a redraw
  return(updated);
}


bool clickFreq(bool shortPress)
{
  if (shortPress) {
    bool updated = updateFrequency(currentFrequency - currentFrequency % getFreqInputStep(), false);

    if (updated) {
      prefsRequestSave(SAVE_CUR_BAND);
      // Clear current station name and information
      clearStationInfo();
      // Check for named frequencies
      identifyFrequency(currentFrequency);
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
  uint8_t squelchValue = currentSquelch[currentMode] & 0x7f;
  uint8_t squelchParam = (currentSquelch[currentMode] & 0x80)? newSNR:newRSSI;
  if(squelchValue)
  {
    if(squelchParam >= squelchValue && muteOn(MUTE_SQUELCH))
    {
      muteOn(MUTE_SQUELCH, false);
    }
    else if(squelchParam < squelchValue && !muteOn(MUTE_SQUELCH))
    {
      muteOn(MUTE_SQUELCH, true);
    }
  }
  else if(muteOn(MUTE_SQUELCH))
  {
    muteOn(MUTE_SQUELCH, false);
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

// Shared by the physical encoder, BLE HID, and ad hoc R/r commands.
bool handleRotation(int16_t enc, int16_t accelerated, bool pressed)
{
  if(!enc || (sleepOn() && sleepModeIdx == SLEEP_LOCKED)) return false;
  bool changed = false;
  if(pressed)
  {
    switch(currentCmd)
    {
      case CMD_NONE:
        currentCmd = CMD_FREQ;
        changed = true;
        break;
      case CMD_FREQ:
        doSelectDigit(enc);
        changed = true;
        break;
      case CMD_SEEK:
        changed = doTune(enc);
        prefsRequestSave(SAVE_CUR_BAND);
        break;
    }
  }
  else
  {
    switch(currentCmd)
    {
      case CMD_NONE:
      case CMD_SCAN:
        changed = doTune(accelerated);
        prefsRequestSave(SAVE_CUR_BAND);
        break;
      case CMD_FREQ:
        changed = doDigit(enc);
        prefsRequestSave(SAVE_CUR_BAND);
        break;
      case CMD_SEEK:
        changed = doSeek(enc, accelerated);
        prefsRequestSave(SAVE_CUR_BAND);
        break;
      default:
        changed = doSideBar(currentCmd, enc, accelerated);
        break;
    }
  }
  elapsedSleep = elapsedCommand = millis();
  return changed;
}

// Shared by the physical button, BLE HID, and ad hoc e/E commands.
bool handleClick(bool shortPress)
{
  elapsedSleep = elapsedCommand = millis();
  if(sleepOn())
  {
    if(currentSleep)
      sleepOn(false);
    else if(sleepModeIdx == SLEEP_UNLOCKED)
    {
      if(shortPress && currentCmd == CMD_NONE)
        currentCmd = CMD_VOLUME;
      else if(currentCmd == CMD_VOLUME)
        clickHandler(currentCmd, shortPress);
    }
    else return false;
  }
  else if(!clickHandler(currentCmd, shortPress))
  {
    if(currentCmd != CMD_NONE)
      currentCmd = CMD_NONE;
    else
      currentCmd = shortPress ? CMD_VOLUME : CMD_MENU;
  }
  // Waking the display or loading schedules can take time.
  elapsedSleep = elapsedCommand = millis();
  return true;
}

//
// Main event loop
//
void loop()
{
  webScreenshotTick();
  offTimerTick();
  bool needRedraw = false;
  uint32_t encCounts = consumeEncoderCounts();
  int16_t encCount = (int16_t)(encCounts & 0xFFFF);
  int16_t encCountAccel = (int16_t)(encCounts >> 16);
  ButtonTracker::State button = pb1.update(digitalRead(ENCODER_PUSH_BUTTON) == LOW);

  if(encCount && sleepOn() && sleepModeIdx == SLEEP_LOCKED)
    encCount = encCountAccel = 0;
  // Keep gesture ownership separate: releasing one control must not clear
  // a held gesture on the other, or affect physical long-press handling.
  static bool physicalPushAndRotate = false;
  if(encCount && button.isPressed) physicalPushAndRotate = true;
  if(!button.isPressed) physicalPushAndRotate = false;

  if(encCount)
    needRedraw |= handleRotation(encCount, encCountAccel, button.isPressed);
  else if(!physicalPushAndRotate && button.isLongPressed)
  {
    sleepOn(!sleepOn());
    elapsedSleep = elapsedCommand = millis();
  }
  else if(!physicalPushAndRotate && (button.wasClicked || button.wasShortPressed))
    needRedraw |= handleClick(button.wasShortPressed);

  // Each source executes independently, so simultaneous inputs are not lost.
  needRedraw |= serialLoop(usbModeIdx);
  needRedraw |= bleLoop(bleModeIdx);
  needRedraw |= tcpLoop(tcpModeIdx);
  uint32_t currentTime = millis();
  const bool heldRotation = physicalPushAndRotate || blePushAndRotate();
  needRedraw |= heldRotation != pushAndRotate;
  pushAndRotate = heldRotation;
  if(pushAndRotate) elapsedSleep = elapsedCommand = currentTime;

  // Disable commands control
  if((currentTime - elapsedCommand) > ELAPSED_COMMAND)
  {
    if(currentCmd != CMD_NONE && currentCmd != CMD_SEEK && currentCmd != CMD_SCAN && currentCmd != CMD_MEMORY)
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
    needRedraw |= (currentMode == FM) && checkRds();
    lastRDSCheck = currentTime;
  }

  // Periodically check schedule
  if((currentTime - lastScheduleCheck) > SCHEDULE_CHECK_TIME)
  {
    needRedraw |= identifyFrequency(currentFrequency, true);
    lastScheduleCheck = currentTime;
  }

  // Tick preferences time, saving changes when there has
  // been no activity for a while
  prefsTickTime();

  // Apply queued web operations, then advance WiFi and time synchronization.
  needRedraw |= webTick();
  needRedraw |= netTickTime();

  // Update clock display
  needRedraw |= clockUpdate();

  // Periodically refresh the main screen
  // This covers the case where there is nothing else triggering a refresh
  if(needRedraw) background_timer = currentTime;
  if((currentTime - background_timer) > BACKGROUND_REFRESH_TIME)
  {
    if(currentCmd == CMD_NONE) needRedraw = true;
    background_timer = currentTime;
  }

  // Expire status messages even when no other display content changes.
  needRedraw |= statusTick(millis());

  // Redraw screen if necessary
  if(needRedraw) drawScreen();

  // Add a small default delay in the main loop
  delay(5);
}
