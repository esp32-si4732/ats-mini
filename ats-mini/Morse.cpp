#include "Common.h"
#include "Menu.h"
#include "Morse.h"

//
// Tuning constants
//
#define MORSE_SAMPLE_MS    6     // Envelope sampling period (ms)
#define MORSE_TEXT_LEN     40    // Visible decoded text length
#define MORSE_SYMBOL_MAX   8     // Maximum dots/dashes per character
#define MORSE_MIN_CONTRAST 6     // Minimum hi-lo spread to treat as keying
#define MORSE_DOT_MIN      20.0f // Minimum adaptive dot length (ms) ~60 WPM
#define MORSE_DOT_MAX      200.0f// Maximum adaptive dot length (ms) ~6 WPM

uint8_t morseModeIdx = MORSE_OFF;

static const char *morseModeDesc[] = { "Off", "RSSI/SNR", "CW (audio)" };

int getTotalMorseModes() { return(ITEM_COUNT(morseModeDesc)); }

const char *morseModeName(uint8_t idx)
{
  return(morseModeDesc[idx <= LAST_ITEM(morseModeDesc) ? idx : 0]);
}

bool morseIsEnabled() { return(morseModeIdx != MORSE_OFF); }

#ifdef MORSE_AUDIO_INPUT
bool morseAudioAvailable() { return(true); }
#else
bool morseAudioAvailable() { return(false); }
#endif

//
// International Morse code table, indexed implicitly by the symbol
// string (dots and dashes). Kept compact as a {code, char} list.
//
struct MorseEntry { const char *code; char ch; };
static const MorseEntry morseTable[] =
{
  {".-",'A'},   {"-...",'B'}, {"-.-.",'C'}, {"-..",'D'},  {".",'E'},
  {"..-.",'F'}, {"--.",'G'},  {"....",'H'}, {"..",'I'},   {".---",'J'},
  {"-.-",'K'},  {".-..",'L'}, {"--",'M'},   {"-.",'N'},   {"---",'O'},
  {".--.",'P'}, {"--.-",'Q'}, {".-.",'R'},  {"...",'S'},  {"-",'T'},
  {"..-",'U'},  {"...-",'V'}, {".--",'W'},  {"-..-",'X'}, {"-.--",'Y'},
  {"--..",'Z'},
  {"-----",'0'},{".----",'1'},{"..---",'2'},{"...--",'3'},{"....-",'4'},
  {".....",'5'},{"-....",'6'},{"--...",'7'},{"---..",'8'},{"----.",'9'},
  {".-.-.-",'.'},{"--..--",','},{"..--..",'?'},{"-..-.",'/'},
  {"-...-",'='}, {".-.-.",'+'}, {"-....-",'-'},{"---...",':'},
  {".--.-.",'@'},{"-.--.",'('}, {"-.--.-",')'},
};

//
// Decoded text rolling buffer
//
static char textBuf[MORSE_TEXT_LEN + 1] = {0};

static void pushChar(char c)
{
  size_t len = strlen(textBuf);
  if(len >= MORSE_TEXT_LEN)
  {
    memmove(textBuf, textBuf + 1, MORSE_TEXT_LEN - 1);
    len = MORSE_TEXT_LEN - 1;
  }
  textBuf[len] = c;
  textBuf[len + 1] = '\0';
}

//
// Decoder state
//
static uint32_t lastSample = 0;   // Time of the last envelope sample
static bool keyDown = false;      // Current key state
static uint32_t edgeTime = 0;     // Time of the last key state change
static float dotLen = 60.0f;      // Adaptive dot (unit) length in ms
static int hiLevel = 0;           // Adaptive signal level
static int loLevel = 0;           // Adaptive noise floor
static char symbols[MORSE_SYMBOL_MAX + 1];
static uint8_t symLen = 0;
static bool spacePending = false; // Whether a word space may still be added

//
// Reset all decoder state
//
void morseReset()
{
  textBuf[0] = '\0';
  keyDown = false;
  edgeTime = millis();
  lastSample = edgeTime;
  dotLen = 60.0f;
  hiLevel = 0;
  loLevel = 0;
  symLen = 0;
  symbols[0] = '\0';
  spacePending = false;

#ifdef MORSE_AUDIO_INPUT
  analogReadResolution(12);
#endif
}

void morseSetup()
{
  morseReset();
}

//
// Read the current normalized envelope value from the active source
//
static int morseReadEnvelope()
{
#ifdef MORSE_AUDIO_INPUT
  if(morseModeIdx == MORSE_AUDIO)
  {
    // Rectified peak-to-peak audio amplitude over a short burst.
    // This is a lightweight envelope detector; a future Goertzel
    // tone filter can replace it for better selectivity.
    int hi = 0, lo = 4095;
    for(int i = 0; i < 16; i++)
    {
      int v = analogRead(MORSE_AUDIO_PIN);
      if(v > hi) hi = v;
      if(v < lo) lo = v;
    }
    // Scale 0..4095 peak-to-peak down to the RSSI-like 0..100 range
    return((hi - lo) * 100 / 4095);
  }
#endif

  // RSSI/SNR source: a fresh read of the SI4732 signal quality
  rx.getCurrentReceivedSignalQuality();
  int level = rx.getCurrentRSSI();
  int s = rx.getCurrentSNR();
  // Blend in SNR so the keying contrast stands out from the noise floor
  return(level + s);
}

//
// Classify the just-ended mark as a dot or dash and append it
//
static void morseEndMark(uint32_t markDur)
{
  bool isDash = markDur > (uint32_t)(2.0f * dotLen);

  // Adapt the unit length toward the observed element duration
  float observed = isDash ? (float)markDur / 3.0f : (float)markDur;
  dotLen = 0.7f * dotLen + 0.3f * observed;
  if(dotLen < MORSE_DOT_MIN) dotLen = MORSE_DOT_MIN;
  if(dotLen > MORSE_DOT_MAX) dotLen = MORSE_DOT_MAX;

  if(symLen < MORSE_SYMBOL_MAX)
  {
    symbols[symLen++] = isDash ? '-' : '.';
    symbols[symLen] = '\0';
  }
  spacePending = true;
}

//
// Decode the accumulated symbol string into a character and emit it
//
static bool morseFlushChar()
{
  if(!symLen) return(false);

  char decoded = '*'; // Unknown sequence marker
  for(unsigned i = 0; i < ITEM_COUNT(morseTable); i++)
  {
    if(!strcmp(symbols, morseTable[i].code))
    {
      decoded = morseTable[i].ch;
      break;
    }
  }

  pushChar(decoded);
  symLen = 0;
  symbols[0] = '\0';
  return(true);
}

//
// Periodic processing, returns TRUE when the decoded text changed
//
bool morseTickTime()
{
  if(morseModeIdx == MORSE_OFF) return(false);

  uint32_t now = millis();
  if((now - lastSample) < MORSE_SAMPLE_MS) return(false);
  lastSample = now;

  int level = morseReadEnvelope();

  // Track the adaptive signal and noise levels with a slow decay so the
  // threshold follows changing band conditions
  if(level > hiLevel) hiLevel = level; else hiLevel--;
  if(level < loLevel) loLevel = level; else loLevel++;
  if(hiLevel < loLevel) hiLevel = loLevel;

  int contrast = hiLevel - loLevel;
  int threshold = (hiLevel + loLevel) / 2;
  int hyst = contrast / 4;

  // Decide the new key state with hysteresis; require a minimum contrast
  // so plain noise is not decoded as keying
  bool newKey;
  if(contrast < MORSE_MIN_CONTRAST)
    newKey = false;
  else if(keyDown)
    newKey = level > (threshold - hyst);
  else
    newKey = level > (threshold + hyst);

  bool changed = false;

  if(newKey != keyDown)
  {
    uint32_t dur = now - edgeTime;
    edgeTime = now;

    if(keyDown)
    {
      // Mark (tone) just ended
      morseEndMark(dur);
    }
    keyDown = newKey;
  }
  else if(!keyDown && symLen)
  {
    // Idle gap handling: end of character / word
    uint32_t gap = now - edgeTime;
    if(gap > (uint32_t)(2.0f * dotLen))
      changed |= morseFlushChar();
  }
  else if(!keyDown && spacePending)
  {
    // Word space after a long silence following a decoded character
    uint32_t gap = now - edgeTime;
    if(gap > (uint32_t)(5.0f * dotLen))
    {
      pushChar(' ');
      spacePending = false;
      changed = true;
    }
  }

  return(changed);
}

const char *morseGetText() { return(textBuf); }
