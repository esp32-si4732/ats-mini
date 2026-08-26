#ifndef JSONAPI_H
#define JSONAPI_H

#include <Arduino.h>
#include "Common.h"

#define JSON_API_VERSION "1.1"

// Sécurité anti-modification accidentelle.
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

// Process incoming JSON command from serial.
// Returns REMOTE_* event flags, same spirit as remoteDoCommand().
int jsonProcessCommand(Stream* stream);

#endif // JSONAPI_H
