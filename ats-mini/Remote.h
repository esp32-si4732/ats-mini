#ifndef REMOTE_H
#define REMOTE_H

typedef struct {
  uint32_t remoteTimer = millis();
  uint8_t remoteSeqnum = 0;
  bool remoteLogOn = false;
  // Cursor for the chunked "$" memory dump (-1 = inactive). The dump emits one
  // populated slot per loop tick so a BLE transfer does not block the main loop.
  int16_t memoryDumpSlot = -1;
} RemoteState;

void remoteTickTime(Stream* stream, RemoteState* state);
void remoteMemoryDumpTick(Stream* stream, RemoteState* state);
int remoteDoCommand(Stream* stream, RemoteState* state, char key);
int serialLoop(uint8_t usbMode);
bool serialConsumeAbortPending(uint8_t usbMode);

#endif
