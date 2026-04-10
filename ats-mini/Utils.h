#ifndef UTILS_H
#define UTILS_H

#include "Common.h"

#define MUTE_FORCE   1
#define MUTE_MAIN    2
#define MUTE_SQUELCH 3
#define MUTE_TEMP    4

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
bool muteOn(uint8_t mode, int x = 2);

// Wall clock functions
const char *clockGet();
bool clockAvailable();
bool clockGetHM(uint8_t *hours, uint8_t *minutes);
bool clockGetHMS(uint8_t *hours, uint8_t *minutes, uint8_t *seconds);
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

// Utility Channels
struct UtilFreq {
    uint32_t freq;    // Hz
    uint8_t mode;     // AM, USB, LSB
    const char* name; // Station Name
    const char* cat;  // Category
    const char* note; // Short listening hint
    uint8_t startHour; // Best listening window start (local hour)
    uint8_t endHour;   // Best listening window end (local hour)
    uint8_t weight;    // Recommendation priority
};

int getUtilFreqCount();
const UtilFreq* getUtilData(int idx);
int getUtilityVisibleCount();
const UtilFreq* getUtilityVisibleData(int idx);
int getUtilityWrapIndex(int current, int delta);
void utilityCycleView();
const char* getUtilityViewLabel();
const char* getUtilityFilterLabel();
int getUtilityCategoryCount();
const char* getUtilityCategoryName(int idx);
int getUtilityCurrentCategory();
void utilitySetCurrentCategory(int idx);
void utilitySyncSelection(int *idx);
void utilitySetDefaultView();

void utilitySavePrefs(bool openPrefs = true);
void utilityLoadPrefs(bool openPrefs = true);

int getUtilityRecommendationCount();
const UtilFreq* getUtilityRecommendation(int idx);

#endif // UTILS_H
