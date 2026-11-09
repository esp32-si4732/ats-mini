#include "Common.h"
#include "Utils.h"
#include "Menu.h"

#define SCAN_POINTS      200 // Number of frequencies to scan

#define SCAN_OFF    0   // Scanner off, no data
#define SCAN_RUN    1   // Scanner running
#define SCAN_DONE   2   // Scanner done, valid data in scanData[]

struct ScanPoint
{
  uint8_t rssi;
  uint8_t snr;
};

static ScanPoint *scanData = nullptr;

static uint32_t scanTime = millis();
static uint32_t scanTuneStarted;
static uint8_t  scanStatus = SCAN_OFF;

static uint32_t scanStartFreq; // Hz
static uint32_t scanStep;      // Hz
static uint16_t scanCount;
static uint8_t  scanMinRSSI;
static uint8_t  scanMaxRSSI;
static uint8_t  scanMinSNR;
static uint8_t  scanMaxSNR;

static inline uint8_t min(uint8_t a, uint8_t b) { return(a<b? a:b); }
static inline uint8_t max(uint8_t a, uint8_t b) { return(a>b? a:b); }

float scanGetRSSI(uint32_t freq)
{
  // Input frequency must be in range of existing data
  if((scanStatus!=SCAN_DONE) || (freq<scanStartFreq) || ((freq-scanStartFreq)/scanStep>=scanCount))
    return(0.0);

  uint8_t result = scanData[(freq - scanStartFreq) / scanStep].rssi;
  return((result - scanMinRSSI) / (float)(scanMaxRSSI - scanMinRSSI + 1));
}

float scanGetSNR(uint32_t freq)
{
  // Input frequency must be in range of existing data
  if((scanStatus!=SCAN_DONE) || (freq<scanStartFreq) || ((freq-scanStartFreq)/scanStep>=scanCount))
    return(0.0);

  uint8_t result = scanData[(freq - scanStartFreq) / scanStep].snr;
  return((result - scanMinSNR) / (float)(scanMaxSNR - scanMinSNR + 1));
}

static void scanInit(uint32_t centerFreq, uint32_t step)
{
  scanStep    = step;
  scanCount   = 0;
  scanMinRSSI = 255;
  scanMaxRSSI = 0;
  scanMinSNR  = 255;
  scanMaxSNR  = 0;
  scanStatus  = SCAN_RUN;
  scanTime    = millis();

  const Band *band = getCurrentBand();
  int64_t freq = ((int64_t)(centerFreq / scanStep) - SCAN_POINTS / 2) * scanStep;
  int64_t span = (int64_t)scanStep * (SCAN_POINTS - 1);

  // Adjust to band boundaries
  if(freq + span > band->maximumFreq)
    freq = band->maximumFreq - span;
  if(freq < band->minimumFreq)
    freq = band->minimumFreq;
  scanStartFreq = freq;

  // Clear scan data
  memset(scanData, 0, SCAN_POINTS * sizeof(*scanData));

  // A preceding confirmed tune has acknowledged STC. Start our own tune
  // before waiting for completion, even if already at the first frequency.
  scanTuneStarted = millis();
  rx.setFrequency(freqFromHz(scanStartFreq, currentMode));
}

static bool scanTickTime()
{
  // Scan must be on
  if((scanStatus!=SCAN_RUN) || (scanCount>=SCAN_POINTS)) return(false);

  if(consumeAbortPending() || millis() - scanTuneStarted >= TUNE_TIMEOUT_MS)
  {
    scanStatus = SCAN_DONE;
    return(false);
  }

  // Wait for the right time
  if(millis() - scanTime < TUNE_POLL_INTERVAL_MS) return(true);

  // This is our current frequency to scan
  uint32_t freq = scanStartFreq + scanStep * scanCount;

  // Poll for the tuning status
  if(!rx.pollTuneStatus())
  {
    scanTime = millis();
    return(true);
  }

  // Do not attach measurements to a stale or unexpected frequency.
  if(rx.getCurrentFrequency() != freqFromHz(freq, currentMode))
  {
    scanStatus = SCAN_DONE;
    return(false);
  }

  // Measure RSSI/SNR values
  rx.getCurrentReceivedSignalQuality();
  scanData[scanCount].rssi = rx.getCurrentRSSI();
  scanData[scanCount].snr  = rx.getCurrentSNR();

  // Measure range of values
  scanMinRSSI = min(scanData[scanCount].rssi, scanMinRSSI);
  scanMaxRSSI = max(scanData[scanCount].rssi, scanMaxRSSI);
  scanMinSNR  = min(scanData[scanCount].snr, scanMinSNR);
  scanMaxSNR  = max(scanData[scanCount].snr, scanMaxSNR);

  // Next frequency to scan
  freq += scanStep;

  // Set next frequency to scan or expire scan
  if((++scanCount >= SCAN_POINTS) || !isFreqInBand(getCurrentBand(), freq) || consumeAbortPending())
    scanStatus = SCAN_DONE;
  else
  {
    scanTuneStarted = millis();
    rx.setFrequency(freqFromHz(freq, currentMode));
  }

  // Save last scan time
  scanTime = millis() - TUNE_POLL_INTERVAL_MS;

  // Return current scan status
  return(scanStatus==SCAN_RUN);
}

//
// Run entire scan once
//
void scanRun(uint32_t centerFreq, uint32_t step)
{
  // Allocate once and retain the samples for drawing after the scan.
  if(!scanData)
  {
    scanData = static_cast<ScanPoint *>(ps_malloc(SCAN_POINTS * sizeof(*scanData)));
    if(!scanData)
      return;
  }

  // Mute the audio
  muteOn(MUTE_TEMP, true);
  // Flag is set by rotary encoder and cleared on seek/scan entry
  seekStop = false;
  // Save current frequency
  uint32_t curFreq = currentFrequency;
  // Scan receiver frequencies without the SSB tuning or calibration offset.
  if(isSSB()) rx.setSSBBfo(0);
  // Scan the whole range
  for(scanInit(centerFreq, step) ; scanTickTime();) delay(1);
  // Restore current frequency
  if(isSSB()) rx.setFrequencyConfirmed(getReceiverFrequency(curFreq));
  // Restore the derived BFO and calibration before unmuting.
  updateFrequency(curFreq, false);
  // Unmute the audio
  muteOn(MUTE_TEMP, false);
}
