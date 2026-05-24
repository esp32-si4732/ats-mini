#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>

void webInit(void);
const String webConfigPage(void);
String getWiFiScanHidden(void);

#endif
