#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

// Run all periodic housekeeping tasks: RSSI/SNR polling, RDS, NTP,
// preferences save, network reconnect, clock update, background timer.
// Returns true if screen needs redraw.
bool runScheduler(uint32_t currentTime);

#endif
