#include "EEPROM.h"
#include "Common.h"
#include "Storage.h"
#include "Themes.h"
#include "Menu.h"
#include <LittleFS.h>

#define STORE_TIME    10000 // Time of inactivity to start writing EEPROM

#define EEPROM_BASE_ADDR  0x000
#define EEPROM_SETM_ADDR  0x080
#define EEPROM_SET_ADDR   0x100
#define EEPROM_SETP_ADDR  0x120
#define EEPROM_VER_ADDR   0x1F0

static bool showEepromFlag   = false; // TRUE: Writing to EEPROM
static bool itIsTimeToSave   = false; // TRUE: Need to save to EEPROM
static bool itIsTimeToUpdate = false; // TRUE: Need to update EEPROM
static uint32_t storeTime    = millis();

// Buffer used to stage EEPROM updates
static uint8_t updateBuf[EEPROM_SIZE];

// To store any change into the EEPROM, we need at least STORE_TIME
// milliseconds of inactivity.
void eepromRequestSave(bool now)
{
  // Underflow is ok here, see eepromTickTime
  storeTime = millis() - (now? STORE_TIME : 0);
  itIsTimeToSave = true;
}

// Buffer new EEPROM contents (possibly from a different thread)
// and request writing EEPROM (in the main thread).
bool eepromRequestUpdate(const uint8_t *eepromUpdate, uint32_t size)
{
  if(size!=sizeof(updateBuf)) return(false);

  memcpy(updateBuf, eepromUpdate, sizeof(updateBuf));
  itIsTimeToUpdate = true;
  return(true);
}

void eepromTickTime()
{
  // Update EEPROM if requested
  if(itIsTimeToUpdate)
  {
    eepromWriteBinary(updateBuf, sizeof(updateBuf));
    eepromLoadConfig();
    selectBand(bandIdx, false);
    rx.setVolume(volume);
    itIsTimeToUpdate = false;
    itIsTimeToSave = false;
  }

  // Save configuration if requested
  if(itIsTimeToSave && ((millis() - storeTime) >= STORE_TIME))
  {
    eepromSaveConfig();
    storeTime = millis();
    itIsTimeToSave = false;
  }
}

// Return true if EEPROM has been written
bool eepromIsWritten()
{
  bool result = showEepromFlag;
  showEepromFlag = false;
  return(result);
}

// Indirectly forces the reset by setting EEPROM_VERSION = 0, which gets
// detected in the subsequent check for EEPROM_VERSION.
// NOTE: EEPROM reset is recommended after firmware updates!
void eepromInvalidate()
{
  // Use EEPROM.begin(EEPROM_SIZE) before use and EEPROM.end() after
  // use to free up memory and avoid memory leaks
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(EEPROM_BASE_ADDR, 0x00);
  EEPROM.write(EEPROM_VER_ADDR + 2, 0x01);
  EEPROM.commit();
  EEPROM.end();
}

// Return true first time after the settings have been reset
bool eepromFirstRun()
{
  EEPROM.begin(EEPROM_SIZE);
  bool firstRun = EEPROM.read(EEPROM_VER_ADDR + 2);
  if(firstRun) EEPROM.write(EEPROM_VER_ADDR + 2, 0x00);
  EEPROM.end();

  return(firstRun);
}

// Check EEPROM contents against EEPROM_VERSION
bool eepromVerify(const uint8_t *buf)
{
  uint8_t  appId;
  uint16_t appVer;

  if(buf)
  {
    appId   = buf[EEPROM_BASE_ADDR];
    appVer  = buf[EEPROM_VER_ADDR] << 8;
    appVer |= buf[EEPROM_VER_ADDR + 1];
  }
  else
  {
    EEPROM.begin(EEPROM_SIZE);
    appId   = EEPROM.read(EEPROM_BASE_ADDR);
    appVer  = EEPROM.read(EEPROM_VER_ADDR) << 8;
    appVer |= EEPROM.read(EEPROM_VER_ADDR + 1);
    EEPROM.end();
  }

  return(appId==EEPROM_VERSION);
}

// Store current receiver configuration into the EEPROM.
// @@@ FIXME: Use EEPROM.update() to avoid writing the same
//            data in the same memory position. It will save
//            unnecessary recording.
void eepromSaveConfig()
{
  // G8PTN: For SSB ensures BFO value is valid with respect to
  // bands[bandIdx].currentFreq = currentFrequency
  int16_t currentBFOs = currentBFO % 1000;
  int addr = EEPROM_BASE_ADDR;

  EEPROM.begin(EEPROM_SIZE);

  EEPROM.write(addr++, EEPROM_VERSION);     // Stores the EEPROM_VERSION;
  EEPROM.write(addr++, volume);             // Stores the current Volume
  EEPROM.write(addr++, bandIdx);            // Stores the current band
  EEPROM.write(addr++, wifiModeIdx);        // Stores WiFi connection mode
  EEPROM.write(addr++, currentMode);        // Stores the current mode (FM / AM / LSB / USB). Now per mode, leave for compatibility
  EEPROM.write(addr++, currentBFOs >> 8);   // G8PTN: Stores the current BFO % 1000 (HIGH byte)
  EEPROM.write(addr++, currentBFOs & 0XFF); // G8PTN: Stores the current BFO % 1000 (LOW byte)
  EEPROM.commit();

  // G8PTN: Commented out the assignment
  // - The line appears to be required to ensure the bands[bandIdx].currentFreq = currentFrequency
  // - Updated main code to ensure that this should occur as required with frequency, band or mode changes
  // - The EEPROM reset code now calls saveAllReceiverInformation(), which is the correct action, this line
  //   must be disabled otherwise bands[bandIdx].currentFreq = 0 (where bandIdx = 0; by default) on EEPROM reset
  //bands[bandIdx].currentFreq = currentFrequency;

  // Store current band settings
  for(int i=0 ; i<getTotalBands() ; i++)
  {
    EEPROM.write(addr++, bands[i].currentFreq >> 8);   // Stores the current Frequency HIGH byte for the band
    EEPROM.write(addr++, bands[i].currentFreq & 0xFF); // Stores the current Frequency LOW byte for the band
    EEPROM.write(addr++, bands[i].currentStepIdx);     // Stores current step of the band
    EEPROM.write(addr++, bands[i].bandwidthIdx);       // Stores bandwidth index
    EEPROM.commit();
  }

  // Store current memories
  addr = EEPROM_SETM_ADDR;
  for(int i=0 ; i<getTotalMemories() ; i++)
  {
    EEPROM.write(addr++, memories[i].freq >> 8);       // Stores frequency HIGH byte
    EEPROM.write(addr++, memories[i].freq & 0xFF);     // Stores frequency LOW byte
    EEPROM.write(addr++, memories[i].mode);            // Stores modulation
    EEPROM.write(addr++, memories[i].band);            // Stores band index
    EEPROM.commit();
  }

  // G8PTN: Added
  addr = EEPROM_SET_ADDR;
  EEPROM.write(addr++, currentBrt >> 8);         // Stores the current Brightness value (HIGH byte)
  EEPROM.write(addr++, currentBrt & 0XFF);       // Stores the current Brightness value (LOW byte)
  EEPROM.write(addr++, FmAgcIdx);                // Stores the current FM AGC/ATTN index value
  EEPROM.write(addr++, AmAgcIdx);                // Stores the current AM AGC/ATTN index value
  EEPROM.write(addr++, SsbAgcIdx);               // Stores the current SSB AGC/ATTN index value
  EEPROM.write(addr++, AmAvcIdx);                // Stores the current AM AVC index value
  EEPROM.write(addr++, SsbAvcIdx);               // Stores the current SSB AVC index value
  EEPROM.write(addr++, AmSoftMuteIdx);           // Stores the current AM SoftMute index value
  EEPROM.write(addr++, SsbSoftMuteIdx);          // Stores the current SSB SoftMute index value
  EEPROM.write(addr++, currentSleep >> 8);       // Stores the current Sleep value (HIGH byte)
  EEPROM.write(addr++, currentSleep & 0XFF);     // Stores the current Sleep value (LOW byte)
  EEPROM.write(addr++, themeIdx);                // Stores the current Theme index value
  EEPROM.write(addr++, rdsModeIdx);              // Stores the current RDS Mode value
  EEPROM.write(addr++, sleepModeIdx);            // Stores the current Sleep Mode value
  EEPROM.write(addr++, (uint8_t)zoomMenu);       // Stores the current Zoom Menu setting
  EEPROM.write(addr++, scrollDirection<0? 1:0);  // Stores the current Scroll setting
  EEPROM.write(addr++, utcOffsetIdx);            // Stores the current UTC Offset
  EEPROM.write(addr++, currentSquelch);          // Stores the current Squelch value
  EEPROM.write(addr++, FmRegionIdx);             // Stores the current FM region value
  EEPROM.write(addr++, uiLayoutIdx);             // Stores the current UI Layout index value
  EEPROM.write(addr++, bleModeIdx);              // Stores the current Bluetooth mode index value
  EEPROM.commit();

  addr = EEPROM_SETP_ADDR;
  for(int i=0 ; i<getTotalBands() ; i++)
  {
    EEPROM.write(addr++, bands[i].usbCal >> 8);    // Stores the current USB Calibration value (HIGH byte) for the band
    EEPROM.write(addr++, bands[i].usbCal & 0xFF);  // Stores the current USB Calibration value (LOW byte) for the band
    EEPROM.write(addr++, bands[i].lsbCal >> 8);    // Stores the current LSB Calibration value (HIGH byte) for the band
    EEPROM.write(addr++, bands[i].lsbCal & 0xFF);  // Stores the current LSB Calibration value (LOW byte) for the band
    EEPROM.write(addr++, bands[i].bandMode);       // Stores the current Mode value for the band
    EEPROM.commit();
  }

  addr = EEPROM_VER_ADDR;
  EEPROM.write(addr++, APP_VERSION >> 8);        // Stores APP_VERSION (HIGH byte)
  EEPROM.write(addr++, APP_VERSION & 0XFF);      // Stores APP_VERSION (LOW byte)
  EEPROM.commit();

  EEPROM.end();

  // Data has been written into EEPROM
  showEepromFlag = true;
}

void eepromLoadConfig()
{
  int addr;

  EEPROM.begin(EEPROM_SIZE);

  addr        = EEPROM_BASE_ADDR + 1;
  volume      = EEPROM.read(addr++);             // Reads stored volume
  bandIdx     = EEPROM.read(addr++);
  wifiModeIdx = EEPROM.read(addr++);             // Reads stored WiFi connection mode
  currentMode = EEPROM.read(addr++);             // Reads stored mode. Now per mode, leave for compatibility
  currentBFO  = EEPROM.read(addr++) << 8;        // Reads stored BFO value (HIGH byte)
  currentBFO |= EEPROM.read(addr++);             // Reads stored BFO value (HIGH byte)

  // Read current band settings
  for(int i=0 ; i<getTotalBands() ; i++)
  {
    bands[i].currentFreq    = EEPROM.read(addr++) << 8;
    bands[i].currentFreq   |= EEPROM.read(addr++);
    bands[i].currentStepIdx = EEPROM.read(addr++);
    bands[i].bandwidthIdx   = EEPROM.read(addr++);
  }

  // Read current memories
  addr = EEPROM_SETM_ADDR;
  for(int i=0 ; i<getTotalMemories() ; i++)
  {
    memories[i].freq  = EEPROM.read(addr++) << 8;
    memories[i].freq |= EEPROM.read(addr++);
    memories[i].mode  = EEPROM.read(addr++);
    memories[i].band  = EEPROM.read(addr++);
  }

  addr = EEPROM_SET_ADDR;
  currentBrt     = EEPROM.read(addr++) << 8;     // Reads stored Brightness value (HIGH byte)
  currentBrt    |= EEPROM.read(addr++);          // Reads stored Brightness value (LOW byte)
  FmAgcIdx       = EEPROM.read(addr++);          // Reads stored FM AGC/ATTN index value
  AmAgcIdx       = EEPROM.read(addr++);          // Reads stored AM AGC/ATTN index value
  SsbAgcIdx      = EEPROM.read(addr++);          // Reads stored SSB AGC/ATTN index value
  AmAvcIdx       = EEPROM.read(addr++);          // Reads stored AM AVC index value
  SsbAvcIdx      = EEPROM.read(addr++);          // Reads stored SSB AVC index value
  AmSoftMuteIdx  = EEPROM.read(addr++);          // Reads stored AM SoftMute index value
  SsbSoftMuteIdx = EEPROM.read(addr++);          // Reads stored SSB SoftMute index value
  currentSleep   = EEPROM.read(addr++) << 8;     // Reads stored Sleep value (HIGH byte)
  currentSleep  |= EEPROM.read(addr++);          // Reads stored Sleep value (LOW byte)
  themeIdx       = EEPROM.read(addr++);          // Reads stored Theme index value
  rdsModeIdx     = EEPROM.read(addr++);          // Reads stored RDS Mode value
  sleepModeIdx   = EEPROM.read(addr++);          // Reads stored Sleep Mode value
  zoomMenu       = (bool)EEPROM.read(addr++);    // Reads stored Zoom Menu setting
  scrollDirection = EEPROM.read(addr++)? -1:1;   // Reads stored Scroll setting
  utcOffsetIdx   = EEPROM.read(addr++);          // Reads the current UTC Offset
  currentSquelch = EEPROM.read(addr++);          // Reads the current Squelch value
  FmRegionIdx    = EEPROM.read(addr++);          // Reads the current FM region value
  FmRegionIdx    = FmRegionIdx >= getTotalFmRegions() ? 0 : FmRegionIdx;
  uiLayoutIdx    = EEPROM.read(addr++);          // Reads stored UI Layout index value
  bleModeIdx     = EEPROM.read(addr++);          // Reads stored Bluetooth mode index value
  bleModeIdx     = bleModeIdx >= getTotalBleModes() ? 0 : bleModeIdx;

  addr = EEPROM_SETP_ADDR;
  for(int i=0 ; i<getTotalBands() ; i++)
  {
    bands[i].usbCal  = EEPROM.read(addr++) << 8;  // Reads stored USB Calibration value (HIGH byte) per band
    bands[i].usbCal |= EEPROM.read(addr++);       // Reads stored USB Calibration value (LOW byte) per band
    bands[i].lsbCal  = EEPROM.read(addr++) << 8;  // Reads stored LSB Calibration value (HIGH byte) per band
    bands[i].lsbCal |= EEPROM.read(addr++);       // Reads stored LSB Calibration value (LOW byte) per band
    // If either calibration value is uninitialised, set to 0
    if (bands[i].usbCal == -1) bands[i].usbCal = 0;
    if (bands[i].lsbCal == -1) bands[i].lsbCal = 0;
    bands[i].bandMode = EEPROM.read(addr++);      // Reads stored Mode value per band
  }

  EEPROM.end();
}

bool diskInit(bool force)
{
  if(force)
  {
    LittleFS.end();
    LittleFS.format();
  }

  bool mounted = LittleFS.begin(false, "/littlefs", 10, "littlefs");

  if(!mounted)
  {
    // Serial.println("Formatting LittleFS...");

    if(!LittleFS.format())
    {
      // Serial.println("ERROR: format failed");
      return(false);
    }

    // Serial.println("Re-mounting LittleFS...");
    mounted = LittleFS.begin(false, "/littlefs", 10, "littlefs");
    if(!mounted)
    {
      // Serial.println("ERROR: remount failed");
      return(false);
    }
  }

  // Serial.println("Mounted LittleFS!");
  return(true);
}

bool eepromReadBinary(uint8_t *buf, uint32_t size)
{
  if(size<EEPROM_SIZE) return(false);

  // Make sure nobody saves
  itIsTimeToSave = false;

  EEPROM.begin(EEPROM_SIZE);

  for(int j=0 ; j<EEPROM_SIZE ; ++j)
    buf[j] = EEPROM.read(j);

  EEPROM.end();
  return(true);
}


bool eepromWriteBinary(const uint8_t *buf, uint32_t size)
{
  if(size>EEPROM_SIZE) return(false);

  // Make sure nobody saves
  itIsTimeToSave = false;

  EEPROM.begin(EEPROM_SIZE);

  for(int j=0 ; j<EEPROM_SIZE ; ++j)
    EEPROM.write(j, buf[j]);

  EEPROM.end();

  return(true);
}
