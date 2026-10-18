#ifndef WEB_H
#define WEB_H

void webInit();
// Main-task processing of queued settings, OTA, and patch operations; returns redraw status.
bool webTick();
// Service screenshot requests at the start of the loop, before blocking operations.
void webScreenshotTick();

#endif // WEB_H
