// Rotary encoder library for Arduino.
#include "Arduino.h"

#ifndef rotary_h
#define rotary_h

#define ENABLE_PULLUPS  // Enable weak pullups

// Values returned by 'process'
#define DIR_NONE 0x0    // No complete step yet
#define DIR_CW   0x10   // Clockwise step
#define DIR_CCW  0x20   // Anti-clockwise step

class Rotary
{
  public:
    Rotary(char, char, bool);
    // Process pin(s)
    unsigned char process();
    // Emit codes at both 00 and 11 instead of at 00 only
    void setHalfStep(bool);
  private:
    unsigned char state;
    unsigned char pin1;
    unsigned char pin2;
    const unsigned char (*table)[4];
};
#endif
