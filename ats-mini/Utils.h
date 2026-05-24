#ifndef UTILS_H
#define UTILS_H

#include "Common.h"

// SSB patch functions
void loadSSB(uint8_t bandwidth, bool draw = true);
void unloadSSB();

// Get firmware version
const char *getVersion(bool shorter = false);

// Hardware info
const char *getMACAddress();

// Convert RSSI to signal strength
int getStrength(int rssi);

// Set, reset, toggle, or query switches
bool sleepOn(int x = 2);

// Wall clock functions
const char *clockGet();
bool clockAvailable();
bool clockGetHM(uint8_t *hours, uint8_t *minutes);
bool clockSet(uint8_t hours, uint8_t minutes, uint8_t seconds = 0);
void clockReset();
bool clockTickTime();
void clockRefreshTime();

// Check if given memory entry belongs to a band
bool isMemoryInBand(const Band *band, const Memory *memory);

// Helpers to convert from/to Hz
uint16_t freqFromHz(uint32_t freq, uint8_t mode);
uint16_t bfoFromHz(uint32_t freq);
uint32_t freqToHz(uint16_t freq, uint8_t mode);

// Check if given frequency belongs to a band
bool isFreqInBand(const Band *band, uint16_t freq);

#endif // UTILS_H
