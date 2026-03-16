#ifndef STORAGE_H
#define STORAGE_H

#include <Preferences.h>

#define STORAGE_PARTITION "settings"

#define SAVE_SETTINGS      0x01
#define SAVE_BANDS         0x02
#define SAVE_MEMORIES      0x04
#define SAVE_CUR_BAND      0x08
#define SAVE_SCAN_CHANNELS 0x10
#define SAVE_VERIFY        0x80
#define SAVE_ALL           (SAVE_SETTINGS|SAVE_BANDS|SAVE_MEMORIES|SAVE_VERIFY)

extern Preferences prefs;

void prefsTickTime();
void prefsInvalidate();
bool prefsAreWritten();
bool nvsErase();

bool diskInit(bool force = false);

void prefsRequestSave(uint32_t what, bool now = false);
void prefsSave(uint32_t items = SAVE_ALL);
bool prefsLoad(uint32_t items = SAVE_ALL);
void prefsSaveBand(uint8_t idx, bool openPrefs = true);
bool prefsLoadBand(uint8_t idx, bool openPrefs = true);
void prefsSaveMemory(uint8_t idx, bool openPrefs = true);
bool prefsLoadMemory(uint8_t idx, bool openPrefs = true);
void prefsSaveScanChannels(uint8_t bandIdx);
bool prefsLoadScanChannels(uint8_t bandIdx);

#endif // STORAGE_H
