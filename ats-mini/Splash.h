#ifndef SPLASH_H
#define SPLASH_H

#include <Arduino.h>

#define SPLASH_PATH      "/splash.png"
#define SPLASH_TEMP_PATH "/splash.png.tmp"

bool splashDraw();
String splashValidate();

#endif // SPLASH_H
