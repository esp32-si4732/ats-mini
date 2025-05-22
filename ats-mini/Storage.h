#ifndef STORAGE_H
#define STORAGE_H

#define EEPROM_SIZE 512

bool eepromFirstRun();
void eepromTickTime();
void eepromInvalidate();
bool eepromVerify(const uint8_t *buf = 0);
void eepromSaveConfig();
void eepromLoadConfig();

bool eepromReadBinary(uint8_t *buf, uint32_t size);
bool eepromWriteBinary(const uint8_t *buf, uint32_t size);

void drawEepromIndicator(int x, int y);

void eepromRequestSave(bool now = false);
void eepromRequestLoad();

#endif // STORAGE_H
