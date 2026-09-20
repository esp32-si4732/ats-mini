#include "Common.h"
#include "Utils.h"
#include "Menu.h"

// Alternative frequency codes, see EN 50067 section 3.2.1.6
#define AF_CODE_MIN         1   // First code that maps to a VHF frequency
#define AF_CODE_MAX       204   // Last code that maps to a VHF frequency
#define AF_CODE_LFMF      250   // Marks an LF/MF frequency, of no use here
#define AF_BASE_FREQ     8750   // Frequency of code zero, in band units
#define AF_CODE_STEP       10   // 100kHz per code, in band units

#define AF_MAX_COUNT       24   // Alternative frequencies kept per station

#define AF_GOOD_RSSI       24   // Signal good enough to stay put (dBuV)
#define AF_GOOD_SNR        12   // Quality good enough to stay put (dB)
#define AF_MARGIN           6   // Improvement expected from an alternative (dB)

#define AF_BAD_TIME     3000U   // Poor reception needed before hunting (msecs)
#define AF_RETRY_TIME   2000U   // Time between two attempts (msecs)
#define AF_SETTLE_TIME     60   // Time for the tuner to settle (msecs)
#define AF_PI_TIME       400U   // Time given to the decoder to report a PI (msecs)
#define AF_PI_POLL         20   // PI code polling interval (msecs)

static uint16_t afList[AF_MAX_COUNT]; // Alternatives, in the units of the band
static uint8_t afCount = 0;           // Alternatives collected so far
static uint8_t afNext  = 0;           // Next alternative to try

static uint32_t afBadTime   = 0;      // Time the reception went bad, or zero
static uint32_t afTriedTime = 0;      // Time of the last attempt

//
// Forget the alternatives, the receiver has left the station
//
void afReset()
{
  afCount = afNext = 0;
  afBadTime = 0;
}

//
// Add a frequency to the list, ignoring duplicates
//
static void afAdd(uint16_t freq)
{
  if(afCount>=AF_MAX_COUNT) return;
  if(!isFreqInBand(getCurrentBand(), freq)) return;

  for(int i=0 ; i<afCount ; i++)
    if(afList[i]==freq) return;

  afList[afCount++] = freq;
}

//
// Collect the alternatives carried by a group 0A. Block C holds two
// codes, either of which can also be a list header or an LF/MF marker.
//
void afCollect()
{
  uint8_t code[2];

  if(!rx.getRdsAFCodes(&code[0], &code[1])) return;

  for(int i=0 ; i<2 ; i++)
  {
    // The code following this marker belongs to an LF/MF frequency
    if(code[i]==AF_CODE_LFMF) break;

    if(code[i]>=AF_CODE_MIN && code[i]<=AF_CODE_MAX)
      afAdd(AF_BASE_FREQ + code[i] * AF_CODE_STEP);
  }
}

//
// Wait for the decoder to report the program identification of the
// frequency the receiver is on right now
//
static uint16_t afReadPi()
{
  uint32_t startTime = millis();

  // Drop whatever the decoder still holds from the previous frequency
  rx.getRdsStatus(0, 1, 0);

  while((millis() - startTime) < AF_PI_TIME)
  {
    rx.getRdsStatus();

    if(rx.getRdsReceived() && rx.getRdsSync())
    {
      uint16_t pi = rx.getRdsPI();
      if(pi) return(pi);
    }

    delay(AF_PI_POLL);
  }

  return(0);
}

//
// Have a look at the next alternative frequency. Returns TRUE when the
// receiver has moved over to it.
//
static bool afTryNext()
{
  uint16_t curFreq = currentFrequency;
  uint16_t freq = afList[afNext];
  uint16_t pi = getRdsPiCode();
  uint8_t newRssi, newSnr;

  afNext = (afNext + 1) % afCount;

  // Nothing to gain from where we already are
  if(freq==curFreq) return(false);

  // Keep quiet while looking around
  muteOn(MUTE_TEMP, true);

  rx.setFrequency(freq);
  delay(AF_SETTLE_TIME);

  rx.getCurrentReceivedSignalQuality();
  newRssi = rx.getCurrentRSSI();
  newSnr  = rx.getCurrentSNR();

  // Only worth a move if it is a clear improvement carrying the same
  // program identification
  if((newRssi>=rssi+AF_MARGIN || newSnr>=snr+AF_MARGIN) &&
     afReadPi()==pi && updateFrequency(freq, false))
  {
    muteOn(MUTE_TEMP, false);
    afBadTime = 0;
    return(true);
  }

  // Back to the frequency we came from
  rx.setFrequency(curFreq);
  delay(AF_SETTLE_TIME);
  muteOn(MUTE_TEMP, false);

  return(false);
}

//
// Watch the reception and follow the station to one of its alternative
// frequencies when the current one fades out. Returns TRUE when the
// screen needs a redraw.
//
bool afTickTime()
{
  uint32_t currentTime = millis();

  // Only an identified FM station can have alternatives
  if(!(getRDSMode() & RDS_AF) || currentMode!=FM || !afCount || !getRdsPiCode())
  {
    afBadTime = 0;
    return(false);
  }

  // Leave the scanners alone while they sweep
  if(currentCmd==CMD_SCAN || memScanRunning()) return(false);

  // Reception is fine, stay where we are
  if(rssi>=AF_GOOD_RSSI && snr>=AF_GOOD_SNR)
  {
    afBadTime = 0;
    return(false);
  }

  // Note when the reception started to go bad
  if(!afBadTime) afBadTime = currentTime;

  // Give the signal a chance to come back, and space out the attempts
  if((currentTime - afBadTime) < AF_BAD_TIME) return(false);
  if((currentTime - afTriedTime) < AF_RETRY_TIME) return(false);

  afTriedTime = currentTime;
  return(afTryNext());
}
