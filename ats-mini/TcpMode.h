#ifndef TCP_MODE_H
#define TCP_MODE_H

#include <stdint.h>

constexpr uint16_t TCP_CONTROL_PORT = 60000;

void tcpStop();
int tcpLoop(uint8_t tcpMode);
bool tcpConsumeAbortPending(uint8_t tcpMode);

#endif
