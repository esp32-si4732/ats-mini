#include "driver/rtc_io.h"
#include "Common.h"
#include "Themes.h"
#include "Button.h"
#include "Utils.h"
#include "Menu.h"
#include "Draw.h"
#include "Storage.h"

// SSB patch for whole SSBRX initialization string
#include "patch_init.h"

extern ButtonTracker pb1;

// Current sleep status, returned by sleepOn()
static bool sleep_on = false;

// Current SSB patch status
static bool ssbLoaded = false;

// Time
static bool clockHasBeenSet = false;
static uint32_t clockTimer  = 0;
static uint8_t clockSeconds = 0;
static uint8_t clockMinutes = 0;
static uint8_t clockHours   = 0;
static char    clockText[8] = {0};

//
// Get firmware version and build time, as a string
//
const char *getVersion(bool shorter)
{
  static char versionString[35] = "\0";

  sprintf(versionString, "%s%sF/W: v%1.1d.%2.2d %s",
    shorter ? "" : RECEIVER_NAME,
    shorter ? "" : " ",
    VER_APP / 100,
    VER_APP % 100,
    __DATE__
  );

  return(versionString);
}

//
// Get MAC address
//
const char *getMACAddress()
{
  static char macString[20] = "\0";

  if(!macString[0])
  {
    uint64_t mac = ESP.getEfuseMac();
    sprintf(
      macString,
      "%02X:%02X:%02X:%02X:%02X:%02X",
      (uint8_t)mac,
      (uint8_t)(mac >> 8),
      (uint8_t)(mac >> 16),
      (uint8_t)(mac >> 24),
      (uint8_t)(mac >> 32),
      (uint8_t)(mac >> 40)
    );
  }
  return(macString);
}

//
// Load SSB patch into SI4735
//
void loadSSB(uint8_t bandwidth, bool draw)
{
  if(!ssbLoaded)
  {
    if(draw) drawMessage("Loading SSB");
    rx.loadPatch(ssb_patch_content, sizeof(ssb_patch_content), bandwidth);
    ssbLoaded = true;
  }
}

void unloadSSB()
{
  // Just mark SSB patch as unloaded
  ssbLoaded = false;
}

//
// Mute sound on (x=1) or off (x=0), or get current status (x=2)
// Do not call this too often because a short PIN_AMP_EN impulse can trigger amplifier mode D,
// see the NS4160 datasheet https://esp32-si4732.github.io/ats-mini/hardware.html#datasheets
//
bool muteOn(uint8_t mode, int x)
{
  // Current mute status
  static bool muted = false;

  // Current squelch status
  static bool squelched = false;

  // Effective mute status
  static bool status = false;

  bool unmute = false;
  bool mute = false;

  if(x==1) {
    status = true;
    switch(mode) {
    case MUTE_FORCE:
      mute = true;
      break;
    case MUTE_MAIN:
      if(!muted && !squelched) {
        mute = true;
      }
      muted = true;
      break;
    case MUTE_SQUELCH:
      if(!muted && !squelched) {
        mute = true;
      }
      squelched = true;
      break;
    case MUTE_TEMP:
      if(!muted && !squelched) {
        mute = true;
      }
      break;
    }
  } else if(x==0) {
    status = false;
    switch(mode) {
    case MUTE_FORCE:
      unmute = true;
      break;
    case MUTE_MAIN:
      if(muted && !squelched) {
        unmute = true;
      }
      muted = false;
      break;
    case MUTE_SQUELCH:
      if(!muted && squelched) {
        unmute = true;
      }
      squelched = false;
      break;
    case MUTE_TEMP:
      if(!muted && !squelched) {
        unmute = true;
      }
      break;
    }
  }

  if(mute) {
    // Disable audio amplifier to silence speaker
    digitalWrite(PIN_AMP_EN, LOW);
    // Activate the mute circuit
    digitalWrite(AUDIO_MUTE, HIGH);
    delay(50);
    rx.setAudioMute(true);
  }

  if(unmute) {
    // Deactivate the mute circuit
    digitalWrite(AUDIO_MUTE, LOW);
    delay(50);
    rx.setAudioMute(false);
    // Enable audio amplifier to restore speaker output
    digitalWrite(PIN_AMP_EN, HIGH);
  }

  switch(mode) {
  case MUTE_MAIN:
    return muted;
  case MUTE_SQUELCH:
    return squelched;
  case MUTE_FORCE:
  case MUTE_TEMP:
  default:
    return status;
  }
}

//
// Turn sleep on (1) or off (0), or get current status (2)
//
bool sleepOn(int x)
{
  if((x==1) && !sleep_on)
  {
    sleep_on = true;
    ledcWrite(PIN_LCD_BL, 0);
    spr.fillSprite(TFT_BLACK);
    spr.pushSprite(0, 0);
    tft.writecommand(ST7789_DISPOFF);
    tft.writecommand(ST7789_SLPIN);

    // Wait till the button is released to prevent immediate wakeup
    while(pb1.update(digitalRead(ENCODER_PUSH_BUTTON) == LOW).isPressed)
      delay(100);

    if(sleepModeIdx == SLEEP_LIGHT)
    {
      // Disable WiFi
      netStop();

      // Unmute squelch
      if(muteOn(MUTE_SQUELCH) && !muteOn(MUTE_MAIN)) muteOn(MUTE_FORCE, false);

      while(true)
      {
        esp_sleep_enable_ext0_wakeup((gpio_num_t)ENCODER_PUSH_BUTTON, LOW);
        rtc_gpio_pullup_en((gpio_num_t)ENCODER_PUSH_BUTTON);
        rtc_gpio_pulldown_dis((gpio_num_t)ENCODER_PUSH_BUTTON);
        esp_light_sleep_start();

        // Waking up here
        if(currentSleep) break; // Short click is enough to exit from sleep if timeout is enabled

        // Wait for a long press, otherwise enter the sleep again
        pb1.reset(); // Reset the button state (its timers could be stale due to CPU sleep)

        bool wasLongPressed = false;
        while(true)
        {
          ButtonTracker::State pb1st = pb1.update(digitalRead(ENCODER_PUSH_BUTTON) == LOW, 0);
          wasLongPressed |= pb1st.isLongPressed;
          if(wasLongPressed || !pb1st.isPressed) break;
          delay(100);
        }

        if(wasLongPressed) break;
      }
      // Reenable the pin as well as the display
      rtc_gpio_pullup_dis((gpio_num_t)ENCODER_PUSH_BUTTON);
      rtc_gpio_pulldown_dis((gpio_num_t)ENCODER_PUSH_BUTTON);
      rtc_gpio_deinit((gpio_num_t)ENCODER_PUSH_BUTTON);
      pinMode(ENCODER_PUSH_BUTTON, INPUT_PULLUP);
      if(muteOn(MUTE_SQUELCH) && !muteOn(MUTE_MAIN)) muteOn(MUTE_FORCE, true);
      sleepOn(false);
      // Enable WiFi
      netInit(wifiModeIdx, false);
    }
  }
  else if((x==0) && sleep_on)
  {
    sleep_on = false;
    tft.writecommand(ST7789_SLPOUT);
    delay(120);
    tft.writecommand(ST7789_DISPON);
    drawScreen();
    ledcWrite(PIN_LCD_BL, currentBrt);
    // Wait till the button is released to prevent the main loop clicks
    pb1.reset(); // Reset the button state (its timers could be stale due to CPU sleep)
    while(pb1.update(digitalRead(ENCODER_PUSH_BUTTON) == LOW, 0).isPressed)
      delay(100);
  }

  return(sleep_on);
}

//
// Set and count time
//

bool clockAvailable()
{
  return(clockHasBeenSet);
}

const char *clockGet()
{
  if(switchThemeEditor())
    return("00:00");
  else
    return(clockHasBeenSet? clockText : NULL);
}

bool clockGetHM(uint8_t *hours, uint8_t *minutes)
{
  if(!clockHasBeenSet) return(false);
  else
  {
    *hours   = clockHours;
    *minutes = clockMinutes;
    return(true);
  }
}

bool clockGetHMS(uint8_t *hours, uint8_t *minutes, uint8_t *seconds)
{
  if(!clockHasBeenSet) return(false);
  else
  {
    *hours   = clockHours;
    *minutes = clockMinutes;
    *seconds = clockSeconds;
    return(true);
  }
}

void clockReset()
{
  clockHasBeenSet = false;
  clockText[0] = '\0';
  clockTimer = 0;
  clockHours = clockMinutes = clockSeconds = 0;
}

static void formatClock(uint8_t hours, uint8_t minutes)
{
  int t = (int)hours * 60 + minutes + getCurrentUTCOffset() * 15;
  t = t < 0? t + 24*60 : t;
  sprintf(clockText, "%02d:%02d", (t / 60) % 24, t % 60);
}

void clockRefreshTime()
{
  if(clockHasBeenSet) formatClock(clockHours, clockMinutes);
}

bool clockSet(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
  // Verify input before setting clock
  if(!clockHasBeenSet && hours < 24 && minutes < 60 && seconds < 60)
  {
    clockHasBeenSet = true;
    clockTimer   = micros();
    clockHours   = hours;
    clockMinutes = minutes;
    clockSeconds = seconds;
    clockRefreshTime();
    identifyFrequency(currentFrequency + currentBFO / 1000);
    return(true);
  }

  // Failed
  return(false);
}

bool clockTickTime()
{
  // Need to set the clock first, then accumulate one second of time
  if(clockHasBeenSet && (micros() - clockTimer >= 1000000))
  {
    uint32_t delta;

    delta = (micros() - clockTimer) / 1000000;
    clockTimer += delta * 1000000;
    clockSeconds += delta;

    if(clockSeconds>=60)
    {
      delta = clockSeconds / 60;
      clockSeconds -= delta * 60;
      clockMinutes += delta;

      if(clockMinutes>=60)
      {
        delta = clockMinutes / 60;
        clockMinutes -= delta * 60;
        clockHours = (clockHours + delta) % 24;
      }

      // Format clock for display and ask for screen update
      clockRefreshTime();
      return(true);
    }
  }

  // No screen update
  return(false);
}

//
// Check if given frequency belongs to given band
//
bool isFreqInBand(const Band *band, uint16_t freq)
{
  return((freq>=band->minimumFreq) && (freq<=band->maximumFreq));
}

//
// Convert a frequency from Hz to mode-specific units
// (TODO: use Hz across the whole codebase)
//
uint16_t freqFromHz(uint32_t freq, uint8_t mode)
{
  return(mode == FM ? freq / 10000 : freq / 1000);
}

//
// Convert a frequency from mode-specific units to Hz
//
uint32_t freqToHz(uint16_t freq, uint8_t mode)
{
  return(mode == FM ? freq * 10000 : freq * 1000);
}

//
// Extract BFO from a frequency in Hz
//
uint16_t bfoFromHz(uint32_t freq)
{
  return(freq % 1000);
}

//
// Check if given memory entry belongs to given band
//
bool isMemoryInBand(const Band *band, const Memory *memory)
{
  uint16_t freq = freqFromHz(memory->freq, memory->mode);
  if(freq<band->minimumFreq) return(false);
  if(freq>band->maximumFreq) return(false);
  if(freq==band->maximumFreq && bfoFromHz(memory->freq)) return(false);
  if(memory->mode==FM && band->bandMode!=FM) return(false);
  if(memory->mode!=FM && band->bandMode==FM) return(false);
  return(true);
}

//
// Get S-level signal strength from RSSI value
//
int getStrength(int rssi)
{
  if(switchThemeEditor()) return(17);

  if(currentMode!=FM)
  {
    // dBuV to S point conversion HF
    if (rssi <=  1) return  1; // S0
    if (rssi <=  2) return  2; // S1
    if (rssi <=  3) return  3; // S2
    if (rssi <=  4) return  4; // S3
    if (rssi <= 10) return  5; // S4
    if (rssi <= 16) return  6; // S5
    if (rssi <= 22) return  7; // S6
    if (rssi <= 28) return  8; // S7
    if (rssi <= 34) return  9; // S8
    if (rssi <= 44) return 10; // S9
    if (rssi <= 54) return 11; // S9 +10
    if (rssi <= 64) return 12; // S9 +20
    if (rssi <= 74) return 13; // S9 +30
    if (rssi <= 84) return 14; // S9 +40
    if (rssi <= 94) return 15; // S9 +50
    if (rssi <= 95) return 16; // S9 +60
    return                 17; //>S9 +60
  }
  else
  {
    // dBuV to S point conversion FM
    if (rssi <=  1) return  1; // S0
    if (rssi <=  2) return  7; // S6
    if (rssi <=  8) return  8; // S7
    if (rssi <= 14) return  9; // S8
    if (rssi <= 24) return 10; // S9
    if (rssi <= 34) return 11; // S9 +10
    if (rssi <= 44) return 12; // S9 +20
    if (rssi <= 54) return 13; // S9 +30
    if (rssi <= 64) return 14; // S9 +40
    if (rssi <= 74) return 15; // S9 +50
    if (rssi <= 76) return 16; // S9 +60
    return                 17; //>S9 +60
  }
}

enum UtilityViewMode
{
  UTIL_VIEW_ALL = 0,
  UTIL_VIEW_NOW = 1,
  UTIL_VIEW_CATEGORY = 2,
};

static const uint8_t UTIL_HOUR_ANY = 0xFF;

static const char *utilityCategories[] =
{
  "TIME",
  "BROADCAST",
  "AERO",
  "MARITIME",
  "DIGITAL",
  "BEACONS",
  "MILITARY",
  "NUMBERS",
  "SPECIAL",
};

//
// Utility frequencies with listening hints
//
static const UtilFreq utilFreqs[] =
{
  { 2500000,  AM,  "WWV/BPM",       "TIME",      "Low-band time signal",       18, 9,  8 },
  { 3330000,  AM,  "CHU Canada",    "TIME",      "Canadian time signal",       18, 9,  8 },
  { 4996000,  AM,  "RWM Russia",    "TIME",      "Russian time signal",        18, 9,  6 },
  { 5000000,  AM,  "WWV/WWVH",      "TIME",      "Reference freq and ticks",   UTIL_HOUR_ANY, UTIL_HOUR_ANY, 10 },
  { 7850000,  AM,  "CHU Canada",    "TIME",      "Useful around dusk",         16, 10, 7 },
  { 10000000, AM,  "WWV/WWVH",      "TIME",      "Best all-round reference",   UTIL_HOUR_ANY, UTIL_HOUR_ANY, 10 },
  { 14670000, AM,  "CHU Canada",    "TIME",      "Daytime path",               8, 18,  6 },
  { 15000000, AM,  "WWV/WWVH",      "TIME",      "Excellent daytime marker",   8, 18,  9 },
  { 20000000, AM,  "WWV/WWVH",      "TIME",      "High-band daytime marker",   9, 17,  6 },
  { 25000000, AM,  "WWV/WWVH",      "TIME",      "Strong only on good days",   10, 16, 5 },

  { 5995000,  AM,  "Radio Mali",    "BROADCAST", "Night shortwave broadcast",  18, 8,  6 },
  { 6070000,  AM,  "Channel 292",   "BROADCAST", "European hobby station",     9, 23,  5 },
  { 6180000,  AM,  "Brazzaville",   "BROADCAST", "African broadcast target",   18, 8,  4 },
  { 7265000,  AM,  "Radio Vanuatu", "BROADCAST", "Pacific regional service",   5, 9,   4 },
  { 9395000,  AM,  "WRMI",          "BROADCAST", "US shortwave relay",         18, 10, 7 },
  { 9580000,  AM,  "Radio Farda",   "BROADCAST", "Middle East service",        15, 22, 5 },
  { 11780000, AM,  "RNA Brasil",    "BROADCAST", "Daytime Latin America",      12, 22, 5 },
  { 13650000, AM,  "Voice of Korea","BROADCAST", "High-band SW broadcast",     9, 17,  4 },
  { 15105000, AM,  "BBC WS relay",  "BROADCAST", "Daytime international",      8, 18,  5 },
  { 17790000, AM,  "CRI relay",     "BROADCAST", "High-band daytime SW",       9, 16,  3 },
  { 3955000,  AM,  "BBC 75m zone",  "BROADCAST", "Nighttime SW window",        18, 8,  4 },
  { 6150000,  AM,  "Europa relay",  "BROADCAST", "Often active in Europe",     17, 9,  5 },
  { 7295000,  AM,  "Regional SW",   "BROADCAST", "Low SW broadcast segment",   17, 8,  4 },
  { 9550000,  AM,  "China Radio",   "BROADCAST", "Strong all-round target",    12, 22, 4 },
  { 11940000, AM,  "BBC/Relay",     "BROADCAST", "Midday SW broadcast",        9, 18,  4 },
  { 15500000, AM,  "Voice Africa",  "BROADCAST", "Higher daytime shortwave",   9, 17,  3 },

  { 2869000,  USB, "Gander Aero",   "AERO",      "N Atlantic night route",     20, 8,  8 },
  { 2962000,  USB, "Shanwick Aero", "AERO",      "Oceanic control at night",   20, 8,  8 },
  { 3413000,  USB, "Shannon Volmet","AERO",      "Weather and met reports",    20, 10, 8 },
  { 4675000,  USB, "Shanwick Aero", "AERO",      "Oceanic night traffic",      19, 10, 7 },
  { 5450000,  USB, "RAF Volmet",    "AERO",      "Military weather voice",     UTIL_HOUR_ANY, UTIL_HOUR_ANY, 8 },
  { 5505000,  USB, "Shannon Volmet","AERO",      "Busy European weather",      UTIL_HOUR_ANY, UTIL_HOUR_ANY, 8 },
  { 6604000,  USB, "Gander Aero",   "AERO",      "Day and dusk traffic",       14, 23, 7 },
  { 6676000,  USB, "Bangkok Volmet","AERO",      "Asian weather broadcast",    0, 12,  5 },
  { 8864000,  USB, "Shanwick Aero", "AERO",      "Good daytime oceanic slot",  8, 18,  7 },
  { 8957000,  USB, "Shannon Volmet","AERO",      "One of the easiest catches", UTIL_HOUR_ANY, UTIL_HOUR_ANY, 9 },
  { 10051000, USB, "Gander Aero",   "AERO",      "Transatlantic daytime",      8, 18,  6 },
  { 11253000, USB, "RAF Volmet",    "AERO",      "Stable daytime weather",     8, 18,  7 },
  { 13264000, USB, "Shannon Volmet","AERO",      "High-band daylight slot",    9, 17,  6 },

  { 2182000,  USB, "Distress 2M",   "MARITIME",  "Voice distress watch",       UTIL_HOUR_ANY, UTIL_HOUR_ANY, 7 },
  { 2187500,  USB, "GMDSS DSC",     "MARITIME",  "Digital maritime calling",   UTIL_HOUR_ANY, UTIL_HOUR_ANY, 7 },
  { 4125000,  USB, "Distress 4M",   "MARITIME",  "Watch freq in USB",          UTIL_HOUR_ANY, UTIL_HOUR_ANY, 6 },
  { 4207500,  USB, "GMDSS DSC",     "MARITIME",  "Digital call watch",         UTIL_HOUR_ANY, UTIL_HOUR_ANY, 6 },
  { 6215000,  USB, "Distress 6M",   "MARITIME",  "Mid-band distress",          UTIL_HOUR_ANY, UTIL_HOUR_ANY, 6 },
  { 6312000,  USB, "GMDSS DSC",     "MARITIME",  "Maritime digital watch",     UTIL_HOUR_ANY, UTIL_HOUR_ANY, 6 },
  { 8291000,  USB, "Distress 8M",   "MARITIME",  "Longer-path maritime",       UTIL_HOUR_ANY, UTIL_HOUR_ANY, 5 },
  { 8414500,  USB, "GMDSS DSC",     "MARITIME",  "Busy when condx are good",   UTIL_HOUR_ANY, UTIL_HOUR_ANY, 5 },
  { 12290000, USB, "Distress 12M",  "MARITIME",  "High-band daytime route",    9, 17,  4 },
  { 12577000, USB, "GMDSS DSC",     "MARITIME",  "Daytime digital watch",      9, 17,  4 },
  { 16420000, USB, "Distress 16M",  "MARITIME",  "Best on strong high bands",  10, 16, 3 },
  { 16804500, USB, "GMDSS DSC",     "MARITIME",  "High-band maritime DSC",     10, 16, 3 },

  { 3573000,  USB, "FT8 80m",       "DIGITAL",   "Night digital watering hole",20, 8,  8 },
  { 5357000,  USB, "FT8 60m",       "DIGITAL",   "Regional digital channel",   18, 9,  6 },
  { 7074000,  USB, "FT8 40m",       "DIGITAL",   "Excellent after sunset",     17, 10, 9 },
  { 10136000, USB, "FT8 30m",       "DIGITAL",   "Often active all day",       UTIL_HOUR_ANY, UTIL_HOUR_ANY, 9 },
  { 14074000, USB, "FT8 20m",       "DIGITAL",   "Best daytime all-rounder",   7, 20, 10 },
  { 14095600, USB, "WSPR 20m",      "DIGITAL",   "Beacon-like weak-signal",    7, 20,  7 },
  { 14230000, USB, "SSTV Call",     "DIGITAL",   "Popular SSTV calling freq",  10, 22, 6 },
  { 18100000, USB, "FT8 17m",       "DIGITAL",   "Good when 20m is lively",    9, 18,  7 },
  { 21074000, USB, "FT8 15m",       "DIGITAL",   "High-band digital action",   9, 17,  6 },
  { 24915000, USB, "FT8 12m",       "DIGITAL",   "Strong only with good condx",10, 16, 4 },
  { 28074000, USB, "FT8 10m",       "DIGITAL",   "Watch during good solar days",10, 16, 5 },
  { 3575000,  USB, "FT4 80m",       "DIGITAL",   "Fast digital around sunset", 20, 8,  6 },
  { 7047500,  USB, "FT4 40m",       "DIGITAL",   "Fast digital on 40m",        17, 10, 7 },
  { 10140000, USB, "FT4 30m",       "DIGITAL",   "FT4 on 30m",                 8, 18,  5 },
  { 14080000, USB, "JS8Call 20m",   "DIGITAL",   "Keyboard weak-signal chat",  7, 21,  6 },
  { 7078000,  USB, "JS8Call 40m",   "DIGITAL",   "Evening JS8 activity",       17, 10, 6 },
  { 18104000, USB, "FT4 17m",       "DIGITAL",   "Fast digital on 17m",        9, 18,  4 },
  { 21140000, USB, "FT4 15m",       "DIGITAL",   "High-band fast digital",     9, 17,  4 },

  { 14100000, USB, "IBP 20m",       "BEACONS",   "NCDXF/IARU beacon chain",    7, 20,  9 },
  { 18110000, USB, "IBP 17m",       "BEACONS",   "Beacon chain on 17m",        8, 18,  7 },
  { 21150000, USB, "IBP 15m",       "BEACONS",   "Beacon chain on 15m",        9, 17,  6 },
  { 24930000, USB, "IBP 12m",       "BEACONS",   "Beacon chain on 12m",        10, 16, 4 },
  { 28200000, USB, "IBP 10m",       "BEACONS",   "Beacon chain on 10m",        10, 16, 4 },

  { 4625000,  USB, "The Buzzer",    "MILITARY",  "UVB-76 style channel",       UTIL_HOUR_ANY, UTIL_HOUR_ANY, 8 },
  { 4724000,  USB, "USAF HFGCS",    "MILITARY",  "Global military net",        UTIL_HOUR_ANY, UTIL_HOUR_ANY, 8 },
  { 6761000,  USB, "USAF HFGCS",    "MILITARY",  "Common daytime slot",        UTIL_HOUR_ANY, UTIL_HOUR_ANY, 8 },
  { 8992000,  USB, "USAF HFGCS",    "MILITARY",  "Another HFGCS channel",      UTIL_HOUR_ANY, UTIL_HOUR_ANY, 8 },
  { 11175000, USB, "USAF HFGCS",    "MILITARY",  "High-band military",         8, 18,  6 },
  { 13200000, USB, "USAF HFGCS",    "MILITARY",  "Best on good daytime paths", 9, 17,  5 },
  { 15016000, USB, "USAF HFGCS",    "MILITARY",  "Upper HF military slot",     9, 16,  4 },

  { 3756000,  USB, "E11 slot",      "NUMBERS",   "Numbers station range",      18, 8,  4 },
  { 4625000,  AM,  "Voice marker",  "NUMBERS",   "Odd utility voice channel",  18, 8,  3 },
  { 4908000,  AM,  "Numbers slot",  "NUMBERS",   "Night utility target",       18, 8,  3 },
  { 6507000,  USB, "Numbers slot",  "NUMBERS",   "Keep an ear on this area",   18, 8,  3 },
  { 11545000, AM,  "Numbers slot",  "NUMBERS",   "Daytime numbers window",     8, 18,  3 },

  { 3340000,  AM,  "HAARP Mon",     "SPECIAL",   "Watch for tests and carriers",18, 8,  3 },
  { 6998000,  AM,  "HAARP Mon",     "SPECIAL",   "Upper monitor frequency",    18, 8,  3 },
  { 27185000, AM,  "CB Ch 19",      "SPECIAL",   "Road traffic chatter",       UTIL_HOUR_ANY, UTIL_HOUR_ANY, 6 },
  { 27025000, AM,  "CB Ch 6",       "SPECIAL",   "High-power AM channel",      11, 22,  5 },
  { 27205000, AM,  "CB Ch 20",      "SPECIAL",   "General AM activity",        UTIL_HOUR_ANY, UTIL_HOUR_ANY, 4 },
  { 5550000,  USB, "ALE watch",     "SPECIAL",   "Listen for ALE bursts",      UTIL_HOUR_ANY, UTIL_HOUR_ANY, 4 },
  { 8038000,  USB, "Weatherfax",    "SPECIAL",   "Marine fax transmissions",   UTIL_HOUR_ANY, UTIL_HOUR_ANY, 4 },
  { 11039000, USB, "Weatherfax",    "SPECIAL",   "HF fax daytime window",      8, 18,  4 },
  { 11253000, USB, "VOLMET backup", "SPECIAL",   "Useful air weather target",  8, 18,  5 },
};

static UtilityViewMode utilityViewMode = UTIL_VIEW_NOW;
static int utilityCategoryIdx = 0;

static int getUtilityLocalHour()
{
  uint8_t hour, minute;
  int localMinutes = 12 * 60;

  if(clockGetHM(&hour, &minute))
  {
    localMinutes = (int)hour * 60 + minute + getCurrentUTCOffset() * 15;
    localMinutes = localMinutes < 0 ? localMinutes + 24 * 60 : localMinutes;
    localMinutes = localMinutes % (24 * 60);
  }

  return localMinutes / 60;
}

static bool utilityMatchesHour(const UtilFreq *entry, int hour)
{
  if(entry->startHour == UTIL_HOUR_ANY || entry->endHour == UTIL_HOUR_ANY)
    return true;

  if(entry->startHour == entry->endHour)
    return true;

  if(entry->startHour < entry->endHour)
    return hour >= entry->startHour && hour < entry->endHour;

  return hour >= entry->startHour || hour < entry->endHour;
}

static int utilityVisibleDbIndex(int visibleIdx)
{
  int match = 0;
  int localHour = getUtilityLocalHour();

  for(int i = 0; i < getUtilFreqCount(); i++)
  {
    bool include = false;

    switch(utilityViewMode)
    {
      case UTIL_VIEW_ALL:
        include = true;
        break;
      case UTIL_VIEW_NOW:
        include = utilityMatchesHour(&utilFreqs[i], localHour);
        break;
      case UTIL_VIEW_CATEGORY:
        include = strcmp(utilFreqs[i].cat, utilityCategories[utilityCategoryIdx]) == 0;
        break;
    }

    if(include)
    {
      if(match == visibleIdx) return i;
      match++;
    }
  }

  return -1;
}

static int utilityScoreEntry(const UtilFreq *entry)
{
  int localHour = getUtilityLocalHour();
  int score = entry->weight * 10;

  if(utilityMatchesHour(entry, localHour))
    score += 30;
  else
    score -= 15;

  score += propagationBandScore(entry->freq) * 18;

  if(currentMode == entry->mode)
    score += 8;

  uint32_t currentHz = freqToHz(currentFrequency, currentMode) + currentBFO;
  uint32_t diff = currentHz > entry->freq ? currentHz - entry->freq : entry->freq - currentHz;
  if(diff < 500000) score += 6;
  else if(diff < 2000000) score += 3;

  return score;
}

int getUtilFreqCount()
{
  return ITEM_COUNT(utilFreqs);
}

const UtilFreq *getUtilData(int idx)
{
  if(idx < 0 || idx >= getUtilFreqCount()) return &utilFreqs[0];
  return &utilFreqs[idx];
}

int getUtilityVisibleCount()
{
  int count = 0;
  int localHour = getUtilityLocalHour();

  for(int i = 0; i < getUtilFreqCount(); i++)
  {
    switch(utilityViewMode)
    {
      case UTIL_VIEW_ALL:
        count++;
        break;
      case UTIL_VIEW_NOW:
        count += utilityMatchesHour(&utilFreqs[i], localHour) ? 1 : 0;
        break;
      case UTIL_VIEW_CATEGORY:
        count += strcmp(utilFreqs[i].cat, utilityCategories[utilityCategoryIdx]) == 0 ? 1 : 0;
        break;
    }
  }

  return count ? count : 1;
}

const UtilFreq *getUtilityVisibleData(int idx)
{
  int dbIdx = utilityVisibleDbIndex(idx);
  if(dbIdx < 0) return &utilFreqs[0];
  return &utilFreqs[dbIdx];
}

int getUtilityWrapIndex(int current, int delta)
{
  int count = getUtilityVisibleCount();
  if(count <= 0) return 0;

  current += delta;
  current = current >= count ? current % count : current;
  current = current < 0 ? count - ((-current) % count) : current;
  return current == count ? 0 : current;
}

void utilitySyncSelection(int *idx)
{
  int count = getUtilityVisibleCount();
  if(!idx || count <= 0) return;
  *idx = getUtilityWrapIndex(*idx, 0);
}

void utilitySetDefaultView()
{
  utilityViewMode = UTIL_VIEW_NOW;
}

void utilityCycleView()
{
  if(utilityViewMode == UTIL_VIEW_ALL)
  {
    utilityViewMode = UTIL_VIEW_NOW;
  }
  else if(utilityViewMode == UTIL_VIEW_NOW)
  {
    utilityViewMode = UTIL_VIEW_CATEGORY;
    utilityCategoryIdx = (utilityCategoryIdx + 1) % getUtilityCategoryCount();
  }
  else
  {
    utilityCategoryIdx++;
    if(utilityCategoryIdx >= getUtilityCategoryCount())
    {
      utilityCategoryIdx = 0;
      utilityViewMode = UTIL_VIEW_ALL;
    }
  }
}

const char *getUtilityViewLabel()
{
  switch(utilityViewMode)
  {
    case UTIL_VIEW_ALL:      return "ALL";
    case UTIL_VIEW_NOW:      return "NOW";
    case UTIL_VIEW_CATEGORY: return "CAT";
  }
  return "ALL";
}

const char *getUtilityFilterLabel()
{
  if(utilityViewMode == UTIL_VIEW_CATEGORY)
    return utilityCategories[utilityCategoryIdx];

  if(utilityViewMode == UTIL_VIEW_NOW)
    return "Best At This Time";

  return "All Utility Entries";
}

int getUtilityCategoryCount()
{
  return ITEM_COUNT(utilityCategories);
}

const char *getUtilityCategoryName(int idx)
{
  if(idx < 0 || idx >= getUtilityCategoryCount()) return utilityCategories[0];
  return utilityCategories[idx];
}

int getUtilityCurrentCategory()
{
  return utilityCategoryIdx;
}

void utilitySetCurrentCategory(int idx)
{
  utilityCategoryIdx = idx < 0 ? 0 : idx % getUtilityCategoryCount();
}

void utilitySavePrefs(bool openPrefs)
{
  if(openPrefs) prefs.begin("settings", false, STORAGE_PARTITION);
  prefs.putUChar("UtilView", (uint8_t)utilityViewMode);
  prefs.putUChar("UtilCat", utilityCategoryIdx);
  if(openPrefs) prefs.end();
}

void utilityLoadPrefs(bool openPrefs)
{
  if(openPrefs) prefs.begin("settings", true, STORAGE_PARTITION);
  utilityViewMode = (UtilityViewMode)prefs.getUChar("UtilView", (uint8_t)utilityViewMode);
  utilityCategoryIdx = prefs.getUChar("UtilCat", utilityCategoryIdx);
  if(utilityViewMode > UTIL_VIEW_CATEGORY) utilityViewMode = UTIL_VIEW_NOW;
  if(utilityCategoryIdx >= getUtilityCategoryCount()) utilityCategoryIdx = 0;
  if(openPrefs) prefs.end();
}

int getUtilityRecommendationCount()
{
  return min(getUtilFreqCount(), 5);
}

const UtilFreq *getUtilityRecommendation(int idx)
{
  if(idx < 0 || idx >= getUtilityRecommendationCount()) return &utilFreqs[0];

  int picked[5] = { -1, -1, -1, -1, -1 };

  for(int slot = 0; slot <= idx; slot++)
  {
    int bestScore = -32768;
    int bestIdx = 0;

    for(int i = 0; i < getUtilFreqCount(); i++)
    {
      bool alreadyPicked = false;
      for(int j = 0; j < slot; j++)
        alreadyPicked |= picked[j] == i;

      if(alreadyPicked) continue;

      int score = utilityScoreEntry(&utilFreqs[i]);
      if(score > bestScore)
      {
        bestScore = score;
        bestIdx = i;
      }
    }

    picked[slot] = bestIdx;
  }

  return &utilFreqs[picked[idx]];
}
