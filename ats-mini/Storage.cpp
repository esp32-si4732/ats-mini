#include "EEPROM.h"
#include "Common.h"
#include "Storage.h"
#include "Themes.h"
#include "Menu.h"

#define EEPROM_SIZE   512
#define STORE_TIME  10000  // Time of inactivity to start writing EEPROM

#define EEPROM_BASE_ADDR  0x000
#define EEPROM_SET_ADDR   0x100
#define EEPROM_SETP_ADDR  0x110
#define EEPROM_VER_ADDR   0x1F0

// Flag indicating EEPROM write request
static bool showEepromFlag = false;
static bool itIsTimeToSave = false;
static uint32_t storeTime = millis();

// To store any change into the EEPROM, we need at least STORE_TIME
// milliseconds of inactivity.
void eepromRequestSave()
{
  storeTime = millis();
  itIsTimeToSave = true;
}

void eepromTickTime()
{
  // Save the current frequency only if it has changed
  if(itIsTimeToSave && ((millis() - storeTime) > STORE_TIME))
  {
    eepromSaveConfig();
    storeTime = millis();
    itIsTimeToSave = false;
  }
}

void drawEepromIndicator(int x, int y)
{
#ifdef THEME_EDITOR
  showEepromFlag = true;
#endif

  // If need to draw EEPROM icon...
  if(showEepromFlag)
  {
    // Draw EEPROM write request icon
    spr.fillRect(x+3, y+2, 3, 5, TH.save_icon);
    spr.fillTriangle(x+1, y+7, x+7, y+7, x+4, y+10, TH.save_icon);
    spr.drawLine(x, y+12, x, y+13, TH.save_icon);
    spr.drawLine(x, y+13, x+8, y+13, TH.save_icon);
    spr.drawLine(x+8, y+13, x+8, y+12, TH.save_icon);

    // Icon drawn
    showEepromFlag = false;
  }
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
  EEPROM.commit();
  EEPROM.end();
}

// Check EEPROM contents against EEPROM_VERSION
bool eepromVerify()
{
  uint8_t  appId;
  uint16_t appVer;

  EEPROM.begin(EEPROM_SIZE);
  appId   = EEPROM.read(EEPROM_BASE_ADDR);
  appVer  = EEPROM.read(EEPROM_VER_ADDR) << 8;
  appVer |= EEPROM.read(EEPROM_VER_ADDR + 1);
  EEPROM.end();

  return(appId==EEPROM_VERSION);
}

// Store current receiver configuration into the EEPROM.
// @@@ FIXME: Use EEPROM.update() to avoid writing the same
//            data in the same memory position. It will save
//            unnecessary recording.
void eepromSaveConfig()
{
  // G8PTN: For SSB ensures BFO value is valid with respect to
  // band[bandIdx].currentFreq = currentFrequency
  int16_t currentBFOs = currentBFO % 1000;
  int addr = EEPROM_BASE_ADDR;

  EEPROM.begin(EEPROM_SIZE);

  EEPROM.write(addr++, EEPROM_VERSION);     // Stores the EEPROM_VERSION;
  EEPROM.write(addr++, volume);             // Stores the current Volume
  EEPROM.write(addr++, bandIdx);            // Stores the current band
  EEPROM.write(addr++, 0x00);               // G8PTN: Not used (was fmRDS)
  EEPROM.write(addr++, currentMode);        // Stores the current Mode (FM / AM / LSB / USB). Now per mode, leave for compatibility
  EEPROM.write(addr++, currentBFOs >> 8);   // G8PTN: Stores the current BFO % 1000 (HIGH byte)
  EEPROM.write(addr++, currentBFOs & 0XFF); // G8PTN: Stores the current BFO % 1000 (LOW byte)
  EEPROM.commit();

  // G8PTN: Commented out the assignment
  // - The line appears to be required to ensure the band[bandIdx].currentFreq = currentFrequency
  // - Updated main code to ensure that this should occur as required with frequency, band or mode changes
  // - The EEPROM reset code now calls saveAllReceiverInformation(), which is the correct action, this line
  //   must be disabled otherwise band[bandIdx].currentFreq = 0 (where bandIdx = 0; by default) on EEPROM reset
  //band[bandIdx].currentFreq = currentFrequency;

  for(int i=0 ; i<=getTotalBands() ; i++)
  {
    EEPROM.write(addr++, (band[i].currentFreq >> 8));   // Stores the current Frequency HIGH byte for the band
    EEPROM.write(addr++, (band[i].currentFreq & 0xFF)); // Stores the current Frequency LOW byte for the band
    EEPROM.write(addr++, band[i].currentStepIdx);       // Stores current step of the band
    EEPROM.write(addr++, band[i].bandwidthIdx);         // table index (direct position) of bandwidth
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
  EEPROM.commit();

  addr = EEPROM_SETP_ADDR;
  for(int i=0 ; i<=getTotalBands() ; i++)
  {
    EEPROM.write(addr++, (band[i].bandCal >> 8));   // Stores the current Calibration value (HIGH byte) for the band
    EEPROM.write(addr++, (band[i].bandCal & 0XFF)); // Stores the current Calibration value (LOW byte) for the band
    EEPROM.write(addr++, band[i].bandMode);      // Stores the current Mode value for the band
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
  volume      = EEPROM.read(addr++);             // Gets the stored volume;
  bandIdx     = EEPROM.read(addr++);
                EEPROM.read(addr++);             // G8PTN: Not used
  currentMode = EEPROM.read(addr++);             // G8PTM: Reads stored Mode. Now per mode, leave for compatibility
  currentBFO  = EEPROM.read(addr++) << 8;        // G8PTN: Reads stored BFO value (HIGH byte)
  currentBFO |= EEPROM.read(addr++);             // G8PTN: Reads stored BFO value (HIGH byte)

  for(int i=0 ; i<=getTotalBands() ; i++)
  {
    band[i].currentFreq    = EEPROM.read(addr++) << 8;
    band[i].currentFreq   |= EEPROM.read(addr++);
    band[i].currentStepIdx = EEPROM.read(addr++);
    band[i].bandwidthIdx   = EEPROM.read(addr++);
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

  addr = EEPROM_SETP_ADDR;
  for(int i=0 ; i<=getTotalBands() ; i++)
  {
    band[i].bandCal  = EEPROM.read(addr++) << 8; // Reads stored Calibration value (HIGH byte) per band
    band[i].bandCal |= EEPROM.read(addr++);      // Reads stored Calibration value (LOW byte) per band
    band[i].bandMode = EEPROM.read(addr++);      // Reads stored Mode value per band
  }

  EEPROM.end();

}
