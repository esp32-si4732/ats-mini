#ifndef BLE_MODE_H
#define BLE_MODE_H

#include "Remote.h"
#include "BleUartPeripheral.h"

void bleInit(uint8_t bleMode);
void bleStop();
int8_t getBleStatus();
void remoteBLETickTime(Stream* stream, RemoteState* state, uint8_t bleMode);
int bleDoCommand(Stream* stream, RemoteState* state, uint8_t bleMode);

extern BleUartPeripheral BLESerial;

#endif
