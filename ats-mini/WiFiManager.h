#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

bool wifiInitConnection(uint8_t netMode, bool showStatus = true);
void netStop(void);
void netTickTime(void);
void netRequestConnect(void);
int8_t getWiFiStatus(void);
char *getWiFiIPAddress(void);

#endif
