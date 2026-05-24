#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

// Mute channels matching the current implementation
#define AM_FORSE   1
#define AM_MAIN    2
#define AM_SQUELCH 3
#define AM_TEMP    4

void audioInit(void);
void audioMuteForce(bool on);
void audioMuteMain(bool on);
void audioSquelchClose(bool on);  // true = close (mute), false = open (unmute)
void audioTempMute(bool on);
bool audioIsMuted(void);
bool audioIsMainMuted(void);
bool audioIsSquelched(void);

#endif
