#ifndef REMOTE_H
#define REMOTE_H

typedef struct {
  uint32_t remoteTimer  = 0;
  uint8_t  remoteSeqnum = 0;
  bool     remoteLogOn  = false; // legacy char-protocol periodic log

  // JSON subscription: send status JSON every jsonSubMs ms (0 = off)
  uint32_t jsonSubMs  = 0;
  uint32_t jsonTimer  = 0;
} RemoteState;

void remoteTickTime(Stream* stream, RemoteState* state);
int  remoteDoCommand(Stream* stream, RemoteState* state, char key);
int  remoteDoJsonCommand(Stream* stream, RemoteState* state, const char* json);
void remoteJsonStatus(Stream* stream, uint8_t seq);
int  serialDoCommand(Stream* stream, RemoteState* state, uint8_t usbMode);
void serialTickTime(Stream* stream, RemoteState* state, uint8_t usbMode);

#endif
