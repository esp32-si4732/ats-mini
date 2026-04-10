#include "Common.h"
#include "Beacons.h"
#include "Menu.h"
#include "Utils.h"
#include "Draw.h"

// NCDXF/IARU International Beacon Project
// 18 beacons, transmitting for 10 seconds each.
// Cycle repeats every 3 minutes.

static const BeaconStation beacons[] = {
    { "4U1UN", "New York, UN", "UN" },
    { "VE8AT", "Canada",       "VE" },
    { "W6WX",  "USA",          "W6" },
    { "KH6RS", "Hawaii",       "KH" },
    { "ZL6B",  "New Zealand",  "ZL" },
    { "VK6RBP","Australia",    "VK" },
    { "JA2IGY","Japan",        "JA" },
    { "RR9O",  "Russia",       "RR" },
    { "VR2B",  "Hong Kong",    "VR" },
    { "4S7B",  "Sri Lanka",    "4S" },
    { "ZS6DN", "South Africa", "ZS" },
    { "5Z4B",  "Kenya",        "5Z" },
    { "4X6TU", "Israel",       "4X" },
    { "OH2B",  "Finland",      "OH" },
    { "CS3B",  "Madeira",      "CS" },
    { "LU4AA", "Argentina",    "LU" },
    { "OA4B",  "Peru",         "OA" },
    { "YV5B",  "Venezuela",    "YV" }
};

// Frequencies in kHz
static const uint32_t beaconFreqs[] = {
    14100, 18110, 21150, 24930, 28200
};

// Band names corresponding to freqs
static const char* bandNames[] = {
    "20m", "17m", "15m", "12m", "10m"
};

static bool beaconModeActive = false;
static int currentBeaconIdx = -1;
static int currentBandIdx = 0; // 0=20m, 1=17m, ...
static int lastSecond = -1;

void beaconInit() {
    beaconModeActive = false;
}

bool isBeaconMode() {
    return beaconModeActive;
}

void toggleBeaconMode() {
    beaconModeActive = !beaconModeActive;
    if (beaconModeActive) {
        // Need to set a SW band to allow USB. 
        // We will hijack the band settings but not save them if possible.
        // Let's assume user is in a SW band or we switch to one.
        
        // Find "20M" band in standard list to set basics
        for(int i=0; i<getTotalBands(); i++) {
             if (strstr(bands[i].bandName, "20M") || strstr(bands[i].bandName, "ALL")) {
                 if (bandIdx != i) selectBand(i, false);
                 break;
             }
        }
        
        // Force USB
        if (currentMode != USB) {
            currentMode = USB;
            // Apply mode change
            rx.setSSB(14000, 14350, 14100, 1, USB);
            // Re-apply bandwidth, etc
            rx.setSSBAudioBandwidth(getCurrentBandwidth()->idx);
            rx.setSSBSidebandCutoffFilter(0);
            rx.setSSBAutomaticVolumeControl(1);
        }

        // Force first frequency
        currentBandIdx = 0;
        updateFrequency(beaconFreqs[currentBandIdx], false);
        
        drawMessage("Beacon Mode ON");
        delay(500);
    }
}

const char* getCurrentBeaconName() {
    if (currentBeaconIdx >= 0 && currentBeaconIdx < 18) {
        return beacons[currentBeaconIdx].callsign;
    }
    return "--";
}

const char* getCurrentBeaconLocation() {
    if (currentBeaconIdx >= 0 && currentBeaconIdx < 18) {
        return beacons[currentBeaconIdx].location;
    }
    return "Wait sync...";
}

// Call this from loop()
void beaconRun() {
    if (!beaconModeActive) return;
    
    uint8_t h, m, s;
    if (!clockGetHMS(&h, &m, &s)) {
        // No clock set
        return;
    }
    
    // Adjust for UTC offset if the clock is local time?
    // clockGet returns local time (UTC + Offset). Beacons are UTC based.
    // So we need to subtract UTC offset.
    // The Utils.cpp keeps raw time in `clockHours` etc, which are set via NTP.
    // Usually NTP sets UTC, but `clockSet` might be receiving local time if offset applied earlier.
    // However, `formatClock` applies offset for display. `clockHours` is usually UTC or base time.
    // Let's assume `clockHours` is UTC if NTP is used correctly, or we reverse the offset.
    // Looking at `formatClock`: t = hours * 60 + minutes + offset.
    // This implies `clockHours` is UTC. Excellent.
    
    int totalSeconds = (m % 3) * 60 + s;
    int slot = totalSeconds / 10; // 0 to 17
    
    // Calculate which beacon is active on the CURRENT band
    // 20m (Band 0): Beacon = Slot
    // 17m (Band 1): Beacon = Slot - 1
    // ...
    // Band k: Beacon = (Slot - k + 18) % 18
    
    int beaconIdx = (slot - currentBandIdx + 18) % 18;
    
    if (beaconIdx != currentBeaconIdx || s != lastSecond) {
        currentBeaconIdx = beaconIdx;
        lastSecond = s;
        
        // Redraw only the beacon info part of the screen if possible,
        // but for now we rely on the main loop redraw or force one if text changes.
        // Actually, Draw.cpp draws station name. We can override `getStationName`.
        
        // If we want to automatically switch BANDS to follow one beacon (e.g. UN), logic is different.
        // Currently implemented: Monitor one band, see which beacon transmits.
    }
}

// Override logic for Draw.cpp
// In Draw.cpp, instead of calling `getStationName`, we might need to hook in.
// Or we can overwrite the `currentStationName` buffer if it existed.
// But `getStationName` reads RDS.
// We can modify `Draw.cpp` to check `isBeaconMode()` and call `getCurrentBeaconName()`.