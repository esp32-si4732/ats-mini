#include "Common.h"
#include "Utils.h"
#include "Menu.h"

#define MEMSCAN_SETTLE_TIME  400U  // Time given to the AGC before measuring (msecs)
#define MEMSCAN_DWELL_TIME  5000U  // Time spent listening to an active channel (msecs)

static bool scanRunning   = false; // TRUE: hopping through the memory slots
static bool scanListening = false; // TRUE: parked on an active channel
static uint8_t scanSlot   = 0;     // Memory slot being checked
static uint32_t scanTime  = 0;     // Time of the last channel change

bool memScanRunning()   { return(scanRunning); }
bool memScanListening() { return(scanListening); }
uint8_t memScanSlot()   { return(scanSlot); }

//
// Check if the current channel carries a signal worth stopping at
//
static bool memScanSignal()
{
  uint8_t minRssi, minSnr;
  getSignalThresholds(&minRssi, &minSnr);

  rx.getCurrentReceivedSignalQuality();
  return(rx.getCurrentRSSI()>=minRssi && rx.getCurrentSNR()>=minSnr);
}

//
// Tune to the first usable memory slot following the given one
//
static bool memScanTune(uint8_t from)
{
  for(int i=1 ; i<=getTotalMemories() ; i++)
  {
    uint8_t slot = (from + i) % getTotalMemories();

    if(memories[slot].freq && tuneToMemory(&memories[slot]))
    {
      scanSlot      = slot;
      scanListening = false;
      scanTime      = millis();
      return(true);
    }
  }

  // There is nothing to scan
  memScanStop();
  return(false);
}

//
// Start scanning at the slot following the last one visited
//
bool memScanStart()
{
  scanRunning = true;
  return(memScanTune(scanSlot));
}

void memScanStop()
{
  scanRunning = scanListening = false;
}

//
// Hop to the next channel when the current one has nothing to offer.
// Returns TRUE when the screen needs a redraw.
//
bool memScanTickTime()
{
  uint32_t currentTime = millis();

  if(!scanRunning) return(false);

  // Let the receiver settle before measuring, then listen for a while
  if((currentTime - scanTime) < (scanListening? MEMSCAN_DWELL_TIME : MEMSCAN_SETTLE_TIME))
    return(false);

  // Stay as long as the channel keeps carrying a signal
  if(memScanSignal())
  {
    scanTime = currentTime;

    if(!scanListening)
    {
      scanListening = true;
      return(true);
    }

    return(false);
  }

  // Channel is quiet, move on
  return(memScanTune(scanSlot));
}
