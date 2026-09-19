#ifndef OTA_H
#define OTA_H

#include <Arduino.h>

enum OtaPhase { OTA_IDLE, OTA_CHECK_QUEUED, OTA_QUEUED, OTA_CONNECTING, OTA_WRITING, OTA_COMPLETE, OTA_REBOOT_PENDING, OTA_FAILED, OTA_CURRENT, OTA_AVAILABLE };

struct OtaStatus
{
  OtaPhase phase;
  String message;
};

// Begin returns false only when busy. Accepted uploads register otaEndUpload
// on disconnect and remain busy through errors until that callback runs.
// imageSize is the exact firmware file size, excluding multipart overhead.
bool otaBegin(size_t imageSize);
bool otaWrite(uint8_t *data, size_t size);
// Validate the completed image and select it for the next boot.
bool otaFinish();
// On disconnect, schedule reboot after success or abort an unfinished upload.
// Failed uploads release the busy flag here so another update can start.
void otaEndUpload();
OtaStatus otaStatus();

bool otaRequestLatest(bool install);
void otaTick();

#endif
