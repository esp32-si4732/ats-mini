#include "Common.h"
#include "Utils.h"
#include "Menu.h"
#include "Draw.h"
#include "Storage.h"

// Tuning delays after rx.setFrequency()
#define TUNE_DELAY_DEFAULT  30
#define TUNE_DELAY_FM       60
#define TUNE_DELAY_AM       80

#define ATS_POLL_TIME       10  // Tuning status polling interval (msecs)
#define ATS_POLL_COUNT      20  // Give up on the tuning status after this many polls
#define ATS_PEAK_DROP        6  // Level drop that tells two stations apart (dB)
#define ATS_NAME_TIME    2000U  // Time given to the decoder to spell out a name (msecs)
#define ATS_NAME_POLL       20  // Station name polling interval (msecs)
#define ATS_NAME_LENGTH      8  // Length of an RDS station name

//
// Channel spacing used for the sweep, in the units of the band
//
static uint16_t autoStoreSpacing(const Band *band)
{
  switch(band->bandType)
  {
    case FM_BAND_TYPE:
      // A 100kHz grid catches both the 100kHz and the 200kHz band plans
      return(10);
    case MW_BAND_TYPE:
    case LW_BAND_TYPE:
      // Follow the current step to stay on the regional 9 or 10kHz grid
      return(getCurrentStep()->spacing>=10? 10 : 9);
    default:
      // International 5kHz channel spacing
      return(5);
  }
}

//
// Wait for the decoder to spell out the name of the station the
// receiver is on, and copy it into the given buffer
//
static bool autoStoreName(char *name, uint8_t size)
{
  uint32_t startTime = millis();

  *name = '\0';

  // Start from a clean slate, the previous station may still be in there
  rx.getRdsStatus(0, 1, 0);
  rx.RdsInit();

  while((millis() - startTime) < ATS_NAME_TIME)
  {
    rx.getRdsStatus();

    if(rx.getRdsReceived() && rx.getRdsSync())
    {
      const char *ps = rx.getRdsStationName();

      // The name comes in four pieces, wait until all of them are in
      if(ps && strlen(ps)>=ATS_NAME_LENGTH)
      {
        uint8_t len = 0;

        for(uint8_t i=0 ; i<ATS_NAME_LENGTH && i<size-1 ; i++)
          name[len++] = (ps[i]>=' ' && ps[i]<='~')? ps[i] : ' ';

        // Drop the padding the broadcasters add
        while(len && name[len-1]==' ') len--;
        name[len] = '\0';

        return(len>0);
      }
    }

    delay(ATS_NAME_POLL);
  }

  return(false);
}

//
// Save a station into the first free memory slot. Returns the number
// of slots taken, or ATS_NO_SLOTS when the memory is full.
//
static int autoStoreSave(uint16_t freq, const char *name)
{
  uint32_t hz = freqToHz(freq, currentMode);
  int slot = -1;

  // Do not store the same station twice
  for(int i=0 ; i<getTotalMemories() ; i++)
    if(memories[i].freq==hz && memories[i].mode==currentMode) return(0);

  for(int i=0 ; i<getTotalMemories() ; i++)
    if(!memories[i].freq)
    {
      slot = i;
      break;
    }

  if(slot<0) return(ATS_NO_SLOTS);

  memset(&memories[slot], 0, sizeof(memories[slot]));
  memories[slot].freq = hz;
  memories[slot].band = bandIdx;
  memories[slot].mode = currentMode;
  strncpy(memories[slot].name, name, sizeof(memories[slot].name) - 1);

  return(1);
}

//
// Sweep the current band from end to end and store every station found
// into the memory, naming it after the RDS name when there is one.
// Returns the number of stations stored, or a negative ATS_* value.
//
int autoStoreRun(uint8_t flags)
{
  const Band *band = getCurrentBand();
  uint16_t spacing = autoStoreSpacing(band);
  bool withNames = (currentMode==FM);
  uint8_t minRssi, minSnr;
  int stored = 0;
  int result = 0;
  int shown = -1;

  // SSB carries no carrier to measure, and the BFO would be lost anyway
  if(isSSB()) return(ATS_BAD_MODE);

  getSignalThresholds(&minRssi, &minSnr);

  // Make room for the stations about to be found
  if(flags & ATS_CLEAR_ALL)
    for(int i=0 ; i<getTotalMemories() ; i++)
      memories[i].freq = 0;
  else if(flags & ATS_REPLACE)
    for(int i=0 ; i<getTotalMemories() ; i++)
      if(memories[i].band==bandIdx) memories[i].freq = 0;

  // Set tuning delay
  rx.setMaxDelaySetFrequency(currentMode==FM? TUNE_DELAY_FM : TUNE_DELAY_AM);
  // Mute the audio
  muteOn(MUTE_TEMP, true);
  // Flag is set by rotary encoder and cleared on seek/scan entry
  seekStop = false;
  // Save current frequency
  uint16_t curFreq = rx.getFrequency();

  // Strongest point of the channel run being followed
  uint16_t peakFreq = 0;
  uint8_t peakRssi = 0;

  for(uint16_t freq=band->minimumFreq ; freq<=band->maximumFreq ; freq+=spacing)
  {
    char text[16];
    bool commit = false;

    rx.setFrequency(freq); // Implies tuning delay

    // Wait for the tuning to complete, but do not get stuck on it
    for(int i=0 ; i<ATS_POLL_COUNT ; i++)
    {
      rx.getStatus(0, 0);
      if(rx.getTuneCompleteTriggered()) break;
      delay(ATS_POLL_TIME);
    }

    // Measure RSSI/SNR values
    rx.getCurrentReceivedSignalQuality();
    uint8_t curRssi = rx.getCurrentRSSI();
    uint8_t curSnr  = rx.getCurrentSNR();

    // A run of channels above the thresholds holds a single station at
    // its strongest point. A deep enough valley starts a new station.
    if(curRssi>=minRssi && curSnr>=minSnr)
    {
      if(!peakFreq || curRssi>peakRssi)
      {
        peakFreq = freq;
        peakRssi = curRssi;
      }
      else if(peakRssi-curRssi>=ATS_PEAK_DROP)
      {
        commit = true;
      }
    }
    else if(peakFreq)
    {
      commit = true;
    }

    if(commit)
    {
      char name[ATS_NAME_LENGTH + 1] = "";

      // Drop back on the peak to pick up the station name
      if(withNames)
      {
        rx.setFrequency(peakFreq);
        autoStoreName(name, sizeof(name));
      }

      int taken = autoStoreSave(peakFreq, name);

      // Nowhere left to store the remaining stations
      if(taken==ATS_NO_SLOTS)
      {
        result = ATS_NO_SLOTS;
        break;
      }

      stored += taken;

      // Keep following the band if the run has not ended yet
      peakFreq = (curRssi>=minRssi && curSnr>=minSnr)? freq : 0;
      peakRssi = peakFreq? curRssi : 0;
    }

    // Show the progress through the band
    int percent = 100L * (freq - band->minimumFreq) / (band->maximumFreq - band->minimumFreq);
    if(percent!=shown)
    {
      sprintf(text, "ATS %d%%", percent);
      drawMessage(text);
      shown = percent;
    }

    // Let the user cut a long sweep short
    if(consumeAbortPending()) break;
  }

  // The last run can reach the end of the band
  if(peakFreq && !result)
  {
    char name[ATS_NAME_LENGTH + 1] = "";

    if(withNames)
    {
      rx.setFrequency(peakFreq);
      autoStoreName(name, sizeof(name));
    }

    int taken = autoStoreSave(peakFreq, name);
    if(taken==ATS_NO_SLOTS) result = ATS_NO_SLOTS;
    else stored += taken;
  }

  // Restore current frequency
  rx.setFrequency(curFreq);
  // Unmute the audio
  muteOn(MUTE_TEMP, false);
  // Restore tuning delay
  rx.setMaxDelaySetFrequency(TUNE_DELAY_DEFAULT);

  // Keep whatever the memory looks like now
  if(stored || (flags & (ATS_CLEAR_ALL | ATS_REPLACE)))
    prefsRequestSave(SAVE_MEMORIES);

  return(result? result : stored);
}
