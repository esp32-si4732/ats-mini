typedef struct {
  uint32_t remoteTimer = millis();
  uint8_t remoteSeqnum = 0;
  bool remoteLogOn = false;
} RemoteState;

void remoteTickTime(Stream* stream, RemoteState* state);
int remoteDoCommand(Stream* stream, RemoteState* state, char key);
