#ifndef REMOTE_H
#define REMOTE_H

typedef struct {
  uint32_t remoteTimer = millis();
  uint8_t remoteSeqnum = 0;
  bool remoteLogOn = false;
} RemoteState;

void remoteTickTime(Stream* stream, RemoteState* state);
int remoteDoCommand(Stream* stream, RemoteState* state, char key);
int serialDoCommand(Stream* stream, RemoteState* state, uint8_t usbMode);
void serialTickTime(Stream* stream, RemoteState* state, uint8_t usbMode);

#endif
