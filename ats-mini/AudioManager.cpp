#include "AudioManager.h"
#include "Common.h"
#include <driver/gpio.h>

//
// Mute state machine with 4 independent channels:
//   Force, Main, Squelch, Temp
//
// The hardware mute is applied only when transitioning between
// "all channels clear" and "any channel active".
//

// Current main mute status
static bool mainMuted = false;

// Current squelch mute status
static bool squelchMuted = false;

// Effective mute status (last requested state)
static bool effectiveMuted = false;

//
// Apply or release hardware mute.
// The delay between AUDIO_MUTE pin toggle and rx.setAudioMute()
// is required for amplifier mode D protection; see the NS4160
// datasheet.
//
static void applyMute(bool mute)
{
  if(mute) {
    if(PIN_AMP_EN >= 0) digitalWrite(PIN_AMP_EN, LOW);
    digitalWrite(AUDIO_MUTE, HIGH);
    delay(50);
    rx.setAudioMute(true);
  } else {
    digitalWrite(AUDIO_MUTE, LOW);
    delay(50);
    rx.setAudioMute(false);
    if(PIN_AMP_EN >= 0) digitalWrite(PIN_AMP_EN, HIGH);
  }
}

//
// Initialize mute pins and state
//
void audioInit(void)
{
  mainMuted = false;
  squelchMuted = false;
  effectiveMuted = false;
  digitalWrite(AUDIO_MUTE, LOW);
  delay(50);
  rx.setAudioMute(false);
  if(PIN_AMP_EN >= 0) digitalWrite(PIN_AMP_EN, HIGH);
}

//
// Force mute — unconditionally overrides all other channels
//
void audioMuteForce(bool on)
{
  if(on) {
    effectiveMuted = true;
    applyMute(true);
  } else {
    effectiveMuted = false;
    applyMute(false);
  }
}

//
// Main mute — user volume toggle
//
void audioMuteMain(bool on)
{
  if(on) {
    effectiveMuted = true;
    if(!mainMuted && !squelchMuted) {
      applyMute(true);
    }
    mainMuted = true;
  } else {
    effectiveMuted = false;
    if(mainMuted && !squelchMuted) {
      applyMute(false);
    }
    mainMuted = false;
  }
}

//
// Squelch — true = close (mute), false = open (unmute)
//
void audioSquelchClose(bool on)
{
  if(on) {
    effectiveMuted = true;
    if(!mainMuted && !squelchMuted) {
      applyMute(true);
    }
    squelchMuted = true;
  } else {
    effectiveMuted = false;
    if(!mainMuted && squelchMuted) {
      applyMute(false);
    }
    squelchMuted = false;
  }
}

//
// Temporary mute — used during seek/scan/band changes
//
void audioTempMute(bool on)
{
  if(on) {
    effectiveMuted = true;
    if(!mainMuted && !squelchMuted) {
      applyMute(true);
    }
  } else {
    effectiveMuted = false;
    if(!mainMuted && !squelchMuted) {
      applyMute(false);
    }
  }
}

//
// Query functions
//
bool audioIsMuted(void)
{
  return effectiveMuted;
}

bool audioIsMainMuted(void)
{
  return mainMuted;
}

bool audioIsSquelched(void)
{
  return squelchMuted;
}
