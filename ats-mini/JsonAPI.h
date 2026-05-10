#ifndef JSONAPI_H
#define JSONAPI_H

#include <Arduino.h>
#include "Common.h"

#define JSON_API_VERSION "1.2"

// Protection anti-modification accidentelle.
// À changer avant compilation si besoin.
#ifndef JSON_API_TOKEN
#define JSON_API_TOKEN "atsmini"
#endif

#ifndef JSON_API_REQUIRE_TOKEN
#define JSON_API_REQUIRE_TOKEN 1
#endif

#ifndef JSON_API_RW_TIMEOUT_MS
#define JSON_API_RW_TIMEOUT_MS 30000UL
#endif

#ifndef JSON_API_SIGNAL_INTERVAL_MS
#define JSON_API_SIGNAL_INTERVAL_MS 500UL
#endif

#ifndef JSON_API_STATE_INTERVAL_MS
#define JSON_API_STATE_INTERVAL_MS 250UL
#endif

// Process incoming JSON command from serial.
// Returns REMOTE_* event flags.
int jsonProcessCommand(Stream* stream);

// Periodic async JSON events.
// Must be called from serialLoop() even when no serial input is available.
void jsonTick(Stream* stream);

#endif // JSONAPI_H
