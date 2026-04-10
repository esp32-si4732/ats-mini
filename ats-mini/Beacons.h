#ifndef BEACONS_H
#define BEACONS_H

#include <Arduino.h>

struct BeaconStation {
    const char* callsign;
    const char* location;
    const char* id; // Short ID like "4U", "UN", etc.
};

// Bands: 20m, 17m, 15m, 12m, 10m
// Freqs: 14.100, 18.110, 21.150, 24.930, 28.200 MHz

void beaconInit();
void beaconRun(); // Call this periodically to update frequency/screen if beacon mode active
bool isBeaconMode();
void toggleBeaconMode();
const char* getCurrentBeaconName();
const char* getCurrentBeaconLocation();

#endif
