#ifndef BLE_MODE_H
#define BLE_MODE_H

#include "Remote.h"
#include "BleUartPeripheral.h"

void bleInit(uint8_t bleMode);
void bleStop();
void bleUpdate(uint8_t bleMode);
bool bleIsPressed(uint8_t bleMode);
int8_t getBleStatus();
void remoteBLETickTime(RemoteState* state, uint8_t bleMode);
int bleDoCommand(RemoteState* state, uint8_t bleMode);

#endif
