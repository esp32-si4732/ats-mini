#include "Common.h"
#include "Storage.h"
#include "Themes.h"
#include "Menu.h"
#include <LittleFS.h>
#include "nvs_flash.h"

// Time of inactivity to start writing preferences
#define STORE_TIME    10000

// Preferences saved here
Preferences prefs;

static uint32_t itIsTimeToSave = 0;       // Preferences to save, or 0 for none
static bool savingPrefsFlag    = false;   // TRUE: Saving preferences
static uint32_t storeTime      = millis();

// To store any change to preferences, we need at least STORE_TIME
// milliseconds of inactivity.
void prefsRequestSave(uint32_t what, bool now)
{
  // Underflow is ok here, see prefsTickTime()
  storeTime = millis() - (now? STORE_TIME : 0);
  itIsTimeToSave |= what;
}

void prefsTickTime()
{
  // Save configuration if requested
  if(itIsTimeToSave && ((millis() - storeTime) >= STORE_TIME))
  {
    prefsSave(itIsTimeToSave);
    storeTime = millis();
    itIsTimeToSave = 0;
  }
}

// Return true if preferences have been written
bool prefsAreWritten()
{
  bool result = savingPrefsFlag;
  savingPrefsFlag = false;
  return(result);
}

// Invlaidate all currently saved preferences
void prefsInvalidate()
{
  static const char *sections[] =
  { "settings", "memories", "bands", "network", 0 };

  // Clear all applicable sections
  for(int j = 0 ; sections[j] ; ++j)
  {
    prefs.begin(sections[j], false, STORAGE_PARTITION);
    prefs.clear();
    prefs.end();
  }
}

struct SavedBand
{
  uint8_t bandMode;       // Band mode (FM, AM, LSB, or USB)
  uint16_t currentFreq;   // Current frequency
  int8_t currentStepIdx;  // Current frequency step
  int8_t bandwidthIdx;    // Index of the table bandwidthFM, bandwidthAM or bandwidthSSB;
  int16_t usbCal;         // USB calibration value
  int16_t lsbCal;         // LSB calibration value
};

void prefsSaveBand(uint8_t idx, bool openPrefs)
{
  SavedBand value;
  char name[32];

  // Will be saving to bands
  if(openPrefs) prefs.begin("bands", false, STORAGE_PARTITION);

  // Compose preference name and value
  sprintf(name, "Band-%d", idx);
  value.currentFreq    = bands[idx].currentFreq;     // Frequency
  value.bandMode       = bands[idx].bandMode;        // Modulation
  value.currentStepIdx = bands[idx].currentStepIdx;  // Step
  value.bandwidthIdx   = bands[idx].bandwidthIdx;    // Bandwidth
  value.usbCal         = bands[idx].usbCal;          // USB Calibration
  value.lsbCal         = bands[idx].lsbCal;          // LSB Calibration

  // Write a preference
  prefs.putBytes(name, &value, sizeof(value));

  // Done with band preferences
  if(openPrefs) prefs.end();
}

bool prefsLoadBand(uint8_t idx, bool openPrefs)
{
  SavedBand value;
  char name[32];

  // Will be loading from bands
  if(openPrefs) prefs.begin("bands", true, STORAGE_PARTITION);

  // Compose preference name
  sprintf(name, "Band-%d", idx);

  // Read preference
  bool result = !!prefs.getBytes(name, &value, sizeof(value));
  if(result)
  {
    bands[idx].currentFreq    = value.currentFreq;    // Frequency
    bands[idx].bandMode       = value.bandMode;       // Modulation
    bands[idx].currentStepIdx = value.currentStepIdx; // Step
    bands[idx].bandwidthIdx   = value.bandwidthIdx;   // Bandwidth
    bands[idx].usbCal         = value.usbCal;         // USB Calibration
    bands[idx].lsbCal         = value.lsbCal;         // LSB Calibration
  }

  // Done with band preferences
  if(openPrefs) prefs.end();

  // Done
  return(result);
}

void prefsSaveMemory(uint8_t idx, bool openPrefs)
{
  char name[32];

  // Will be saving to memories
  if(openPrefs) prefs.begin("memories", false, STORAGE_PARTITION);

  // Compose preference name
  sprintf(name, "Memory-%d", idx);

  // Write a preference
  prefs.putBytes(name, &memories[idx], sizeof(memories[idx]));

  // Done with memory preferences
  if(openPrefs) prefs.end();
}

bool prefsLoadMemory(uint8_t idx, bool openPrefs)
{
  char name[32];

  // Will be loading from memories
  if(openPrefs) prefs.begin("memories", true, STORAGE_PARTITION);

  // Compose preference name
  sprintf(name, "Memory-%d", idx);

  // Write a preference
  bool result = !!prefs.getBytes(name, &memories[idx], sizeof(memories[idx]));

  // Done with memory preferences
  if(openPrefs) prefs.end();

  // Done
  return(result);
}

void prefsSave(uint32_t items)
{
  if(items & SAVE_SETTINGS)
  {
    // Will be saving to settings
    prefs.begin("settings", false, STORAGE_PARTITION);

    // Save main global settings
    prefs.putUChar("Version",  VER_SETTINGS);      // Settings version
    prefs.putUShort("App",     VER_APP);           // Application version
    prefs.putUChar("Volume",   radioState.vol);            // Current radioState.vol
    prefs.putUChar("Band",     bandIdx);           // Current band
    prefs.putUChar("WiFiMode", radioState.wifiMode);       // WiFi connection mode

    // Save additional global settings
    prefs.putUShort("Brightness", radioState.brightness);     // Brightness
    prefs.putUChar("FmAGC",       radioState.fmAgcIdx);       // FM AGC/ATTN
    prefs.putUChar("AmAGC",       radioState.amAgcIdx);       // AM AGC/ATTN
    prefs.putUChar("SsbAGC",      radioState.ssbAgcIdx);      // SSB AGC/ATTN
    prefs.putUChar("AmAVC",       radioState.amAvcIdx);       // AM AVC
    prefs.putUChar("SsbAVC",      radioState.ssbAvcIdx);      // SSB AVC
    prefs.putUChar("AmSoftMute",  radioState.amSoftMuteIdx);  // AM soft mute
    prefs.putUChar("SsbSoftMute", radioState.ssbSoftMuteIdx); // SSB soft mute
    prefs.putUShort("Sleep",      radioState.sleep);   // Sleep delay
    prefs.putUChar("Theme",       themeIdx);       // Color theme
    prefs.putUChar("RDSMode",     radioState.rdsMode);     // RDS mode
    prefs.putUChar("SleepMode",   radioState.sleepMode);   // Sleep mode
    prefs.putUChar("ZoomMenu",    radioState.zoomLevel);       // TRUE: Zoom menu
    prefs.putBool("ScrollDir", radioState.scrollDir<0); // TRUE: Reverse scroll
    prefs.putUChar("UTCOffset",   radioState.utcOffset);   // UTC Offset
    prefs.putUInt("Squelch",      ((uint32_t)radioState.squelch[FM]) |
                                  ((uint32_t)radioState.squelch[LSB] << 8) |
                                  ((uint32_t)radioState.squelch[USB] << 16) |
                                  ((uint32_t)radioState.squelch[AM] << 24)); // Squelch
    prefs.putUChar("FmRegion",    radioState.fmRegionIdx);    // FM region
    prefs.putUChar("UILayout",    radioState.uiLayout);    // UI Layout
    prefs.putUChar("BLEMode",     radioState.bleMode);     // Bluetooth mode
    prefs.putUChar("USBMode",     radioState.usbMode);     // USB mode

    // Done with global settings
    prefs.end();
  }

  if(items & SAVE_BANDS)
  {
    // Will be saving to bands
    prefs.begin("bands", false, STORAGE_PARTITION);
    prefs.putUChar("Version", VER_BANDS);
    // Save band settings
    for(int i=0 ; i<getTotalBands() ; i++) prefsSaveBand(i, false);
    // Done with bands
    prefs.end();
  }
  else if(items & SAVE_CUR_BAND)
  {
    // Save current band only
    prefsSaveBand(bandIdx);
  }

  if(items & SAVE_MEMORIES)
  {
    // Will be saving to memories
    prefs.begin("memories", false, STORAGE_PARTITION);
    prefs.putUChar("Version", VER_MEMORIES);
    // Save current memories
    for(int i=0 ; i<getTotalMemories() ; i++) prefsSaveMemory(i, false);
    // Done with memories
    prefs.end();
  }

  // Preferences have been saved
  savingPrefsFlag = true;
}

bool prefsLoad(uint32_t items)
{
  if(items & SAVE_SETTINGS)
  {
    // Will be loading from settings
    prefs.begin("settings", true, STORAGE_PARTITION);

    // Check currently saved version
    if((items & SAVE_VERIFY) && (prefs.getUChar("Version", 0) != VER_SETTINGS))
    {
      prefs.end();
      return(false);
    }

    // Load main global settings
    radioState.vol         = prefs.getUChar("Volume", radioState.vol);          // Current radioState.vol
    bandIdx        = prefs.getUChar("Band", bandIdx);           // Current band
    radioState.wifiMode    = prefs.getUChar("WiFiMode", radioState.wifiMode);   // WiFi connection mode
    radioState.brightness     = prefs.getUShort("Brightness", radioState.brightness); // Brightness
    radioState.fmAgcIdx       = prefs.getUChar("FmAGC", radioState.fmAgcIdx);         // FM AGC/ATTN
    radioState.amAgcIdx       = prefs.getUChar("AmAGC", radioState.amAgcIdx);         // AM AGC/ATTN
    radioState.ssbAgcIdx      = prefs.getUChar("SsbAGC", radioState.ssbAgcIdx);       // SSB AGC/ATTN
    radioState.amAvcIdx       = prefs.getUChar("AmAVC", radioState.amAvcIdx);         // AM AVC
    radioState.ssbAvcIdx      = prefs.getUChar("SsbAVC", radioState.ssbAvcIdx);       // SSB AVC
    radioState.amSoftMuteIdx  = prefs.getUChar("AmSoftMute", radioState.amSoftMuteIdx);   // AM soft mute
    radioState.ssbSoftMuteIdx = prefs.getUChar("SsbSoftMute", radioState.ssbSoftMuteIdx); // SSB soft mute
    radioState.sleep   = prefs.getUShort("Sleep", radioState.sleep);    // Sleep delay
    themeIdx       = prefs.getUChar("Theme", themeIdx);         // Color theme
    radioState.rdsMode     = prefs.getUChar("RDSMode", radioState.rdsMode);     // RDS mode
    radioState.sleepMode   = prefs.getUChar("SleepMode", radioState.sleepMode); // Sleep mode
    radioState.zoomLevel       = prefs.getUChar("ZoomMenu", radioState.zoomLevel);      // TRUE: Zoom menu
    radioState.scrollDir = prefs.getBool("ScrollDir", radioState.scrollDir<0)? -1:1; // TRUE: Reverse scroll
    radioState.utcOffset   = prefs.getUChar("UTCOffset", radioState.utcOffset); // UTC Offset
    uint32_t squelch = prefs.getUInt("Squelch", ((uint32_t)radioState.squelch[FM]) |
                                                ((uint32_t)radioState.squelch[LSB] << 8) |
                                                ((uint32_t)radioState.squelch[USB] << 16) |
                                                ((uint32_t)radioState.squelch[AM] << 24)); // Squelch
    radioState.squelch[FM]  = squelch & 0xff;
    radioState.squelch[LSB] = (squelch >> 8) & 0xff;
    radioState.squelch[USB] = (squelch >> 16) & 0xff;
    radioState.squelch[AM]  = (squelch >> 24) & 0xff;
    radioState.fmRegionIdx    = prefs.getUChar("FmRegion", radioState.fmRegionIdx);   // FM region
    radioState.uiLayout    = prefs.getUChar("UILayout", radioState.uiLayout);   // UI Layout
    radioState.bleMode     = prefs.getUChar("BLEMode", radioState.bleMode);     // Bluetooth mode
    radioState.usbMode     = prefs.getUChar("USBMode", radioState.usbMode);     // USB mode

    // Done with global settings
    prefs.end();
  }

  if(items & SAVE_BANDS)
  {
    // Will be loading from bands
    prefs.begin("bands", true, STORAGE_PARTITION);

    // Check currently saved version
    if((items & SAVE_VERIFY) && (prefs.getUChar("Version", 0) != VER_BANDS))
    {
      prefs.end();
      return(false);
    }

    // Read band settings
    for(int i=0 ; i<getTotalBands() ; i++) prefsLoadBand(i, false);

    // Done with bands
    prefs.end();
  }
  else if(items & SAVE_CUR_BAND)
  {
    // Load current band only
    prefsLoadBand(bandIdx);
  }

  if(items & SAVE_MEMORIES)
  {
    // Will be loading from memories
    prefs.begin("memories", true, STORAGE_PARTITION);

    // Check currently saved version
    if((items & SAVE_VERIFY) && (prefs.getUChar("Version", 0) != VER_MEMORIES))
    {
      prefs.end();
      return(false);
    }

    // Read all memories
    for(int i=0 ; i<getTotalMemories() ; i++) prefsLoadMemory(i, false);

    // Done with memories
    prefs.end();
  }

  return(true);
}

bool diskInit(bool force)
{
  if(!force)
  {
    prefs.begin("storage", true, STORAGE_PARTITION);
    force = prefs.getUChar("Version", 0) != VER_STORAGE;
    prefs.end();
  }

  if(force)
  {
    LittleFS.end();
    LittleFS.format();
  }

  bool mounted = LittleFS.begin(false, "/littlefs", 10, "littlefs");

  if(!mounted)
  {
    if(!LittleFS.format())
    {
      return(false);
    }

    mounted = LittleFS.begin(false, "/littlefs", 10, "littlefs");
    if(!mounted)
    {
      return(false);
    }
  }

  prefs.begin("storage", false, STORAGE_PARTITION);
  prefs.putUChar("Version", VER_STORAGE);
  prefs.end();

  return(true);
}

bool nvsErase()
{
  return(nvs_flash_erase() == ESP_OK &&
         nvs_flash_init() == ESP_OK &&
         nvs_flash_erase_partition(STORAGE_PARTITION) == ESP_OK &&
         nvs_flash_init_partition(STORAGE_PARTITION) == ESP_OK);
}
