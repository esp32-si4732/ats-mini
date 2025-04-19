#include "Common.h"

// SSB patch for whole SSBRX initialization string
#include "patch_init.h"

// Current mute status, returned by muteOn()
static bool muted = false;

// Volume level when mute is applied
static uint8_t mute_vol_val = 0;

// Current display status, returned by displayOn()
static bool display_on = true;

// Current SSB patch status
static bool ssbLoaded = false;

// Time
static bool clockHasBeenSet  = false;
static uint32_t clockTimer   = 0;
static uint32_t clockSeconds = 0;
static uint32_t clockMinutes = 0;
static uint32_t clockHours   = 0;
static char     clockText[8] = {0};

//
// Get firmware version and build time, as a string
//
const char *getVersion()
{
  static char versionString[25] = "\0";

  if(!versionString[0])
    sprintf(versionString, "F/W: v%1.1d.%2.2d %s",
      APP_VERSION / 100,
      APP_VERSION % 100,
      __DATE__
    );

  return(versionString);
}

//
// Load SSB patch into SI4735
//
void loadSSB(uint8_t bandwidth)
{
  if(!ssbLoaded)
  {
    // You can try rx.setI2CFastModeCustom(700000); or greater value
    rx.setI2CFastModeCustom(400000);
    rx.loadPatch(ssb_patch_content, sizeof(ssb_patch_content), bandwidth);
    rx.setI2CFastModeCustom(100000);
    ssbLoaded = true;
  }
}

void unloadSSB()
{
  // Just mark SSB patch as unloaded
  ssbLoaded = false;
}

//
// Mute sound on (1) or off (0), or get current status (2)
//
bool muteOn(int x)
{
  if((x==0) && muted)
  {
    rx.setVolume(mute_vol_val);
    muted = false;
  }
  else if((x==1) && !muted)
  {
    mute_vol_val = rx.getVolume();
    rx.setVolume(0);
    muted = true;
  }

  return(muted);
}

//
// Turn display on (1) or off (0), or get current status (2)
//
bool displayOn(int x)
{
  if((x==0) && display_on)
  {
    display_on = false;
    ledcWrite(PIN_LCD_BL, 0);
    tft.writecommand(ST7789_DISPOFF);
    tft.writecommand(ST7789_SLPIN);
    delay(120);
  }
  else if((x==1) && !display_on)
  {
    display_on = true;
    tft.writecommand(ST7789_SLPOUT);
    delay(120);
    tft.writecommand(ST7789_DISPON);
    ledcWrite(PIN_LCD_BL, currentBrt);
  }

  return(display_on);
}

//
// Set and count time
//

const char *clockGet()
{
#ifdef THEME_EDITOR
  return("00:00");
#else
  return(clockHasBeenSet? clockText : "");
#endif
}

bool clockSet(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
  // Verify input before setting clock
  if(hours < 24 && minutes < 60 && seconds < 60)
  {
    clockHasBeenSet = true;
    clockTimer   = micros();
    clockHours   = hours;
    clockMinutes = minutes;
    clockSeconds = seconds;
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
      sprintf(clockText, "%02ld:%02ld", clockHours, clockMinutes);
      return(true);
    }
  }

  // No screen update
  return(false);
}

//
// Get S-level signal strength from RSSI value
//
int getStrength(int rssi)
{
#ifdef THEME_EDITOR
  return(17);
#endif
  if(currentMode!=FM)
  {
    // dBuV to S point conversion HF
    if ((rssi <= 1))                  return  1;  // S0
    if ((rssi >  1) and (rssi <=  2)) return  2;  // S1
    if ((rssi >  2) and (rssi <=  3)) return  3;  // S2
    if ((rssi >  3) and (rssi <=  4)) return  4;  // S3
    if ((rssi >  4) and (rssi <= 10)) return  5;  // S4
    if ((rssi > 10) and (rssi <= 16)) return  6;  // S5
    if ((rssi > 16) and (rssi <= 22)) return  7;  // S6
    if ((rssi > 22) and (rssi <= 28)) return  8;  // S7
    if ((rssi > 28) and (rssi <= 34)) return  9;  // S8
    if ((rssi > 34) and (rssi <= 44)) return 10;  // S9
    if ((rssi > 44) and (rssi <= 54)) return 11;  // S9 +10
    if ((rssi > 54) and (rssi <= 64)) return 12;  // S9 +20
    if ((rssi > 64) and (rssi <= 74)) return 13;  // S9 +30
    if ((rssi > 74) and (rssi <= 84)) return 14;  // S9 +40
    if ((rssi > 84) and (rssi <= 94)) return 15;  // S9 +50
    if  (rssi > 94)                   return 16;  // S9 +60
    if  (rssi > 95)                   return 17;  //>S9 +60
  }
  else
  {
    // dBuV to S point conversion FM
    if ((rssi <= 1))                  return  1;  // S0
    if ((rssi >  1) and (rssi <=  2)) return  7;  // S6
    if ((rssi >  2) and (rssi <=  8)) return  8;  // S7
    if ((rssi >  8) and (rssi <= 14)) return  9;  // S8
    if ((rssi > 14) and (rssi <= 24)) return 10;  // S9
    if ((rssi > 24) and (rssi <= 34)) return 11;  // S9 +10
    if ((rssi > 34) and (rssi <= 44)) return 12;  // S9 +20
    if ((rssi > 44) and (rssi <= 54)) return 13;  // S9 +30
    if ((rssi > 54) and (rssi <= 64)) return 14;  // S9 +40
    if ((rssi > 64) and (rssi <= 74)) return 15;  // S9 +50
    if  (rssi > 74)                   return 16;  // S9 +60
    if  (rssi > 76)                   return 17;  //>S9 +60
  }

  return(1);
}
