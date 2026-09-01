#include "Common.h"
#include "Themes.h"
#include "Utils.h"

#define VBAT_MON  4                 // GPIO04 -- Battery Monitor PIN

#define BATT_ADC_READS          10  // ADC reads for average calculation (Maximum value = 16 to avoid rollover in average calculation)
#define BATT_ADC_FACTOR      1.702  // ADC correction factor used for the battery monitor
#define BATT_SOC_LEVEL1      3.680  // Battery SOC voltage for 25%
#define BATT_SOC_LEVEL2      3.780  // Battery SOC voltage for 50%
#define BATT_SOC_LEVEL3      3.880  // Battery SOC voltage for 75%
#define BATT_SOC_HYST_2      0.020  // Battery SOC hyteresis voltage divided by 2

// State machine used for battery state of charge (SOC) detection with
// hysteresis (Default = Illegal state)
static uint8_t batteryState = 255;

// Current battery voltage
static float batteryVolts = 4.0;

//
// Measure and return battery voltage
//
float batteryMonitor()
{
  int i, j;

  // Read ADC multiple times
  for(i=j=0 ; i<BATT_ADC_READS ; i++) j += analogRead(VBAT_MON);

  // Calculate average voltage with correction factor
  batteryVolts = ((float)j / BATT_ADC_READS) * BATT_ADC_FACTOR / 1000;

  // State machine
  // SOC (%)      batteryState
  //  0 to 25           0
  // 25 to 50           1
  // 50 to 75           2
  // 75 to 100          3
  switch(batteryState)
  {
    case 0:
      if      (batteryVolts > (BATT_SOC_LEVEL1 + BATT_SOC_HYST_2)) batteryState = 1;   // State 0 > 1
      break;
    case 1:
      if      (batteryVolts > (BATT_SOC_LEVEL2 + BATT_SOC_HYST_2)) batteryState = 2;   // State 1 > 2
      else if (batteryVolts < (BATT_SOC_LEVEL1 - BATT_SOC_HYST_2)) batteryState = 0;   // State 1 > 0
      break;
    case 2:
      if      (batteryVolts > (BATT_SOC_LEVEL3 + BATT_SOC_HYST_2)) batteryState = 3;   // State 2 > 3
      else if (batteryVolts < (BATT_SOC_LEVEL2 - BATT_SOC_HYST_2)) batteryState = 1;   // State 2 > 1
      break;
    case 3:
      if      (batteryVolts < (BATT_SOC_LEVEL3 - BATT_SOC_HYST_2)) batteryState = 2;   // State 3 > 2
      break;
    default:
      if      (batteryState > 3) batteryState = 0;                                // State (Illegal) > 0
      break;
  }

  // Return current voltage
  return(batteryVolts);
}

//
// Show last measured battery voltage and status at given screen
// coordinates. Return true if voltage was drawn.
//
bool drawBattery(int x, int y)
{
  if(sleepOn()) return false;

  // Measure battery voltage and status
  batteryMonitor();

  // Set display information
  spr.drawRoundRect(x, y + 1, 28, 14, 3, TH.batt_border);
  spr.drawLine(x + 29, y + 5, x + 29, y + 10, TH.batt_border);
  spr.drawLine(x + 30, y + 6, x + 30, y + 9, TH.batt_border);

  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(TH.batt_voltage);

  if(switchThemeEditor())
  {
    // Alternate between five battery states every 10 seconds
    batteryState = (millis() % 50000u) / 10000u;
    batteryVolts = batteryState >= 4 ? 4.5 : 4.0;
  }

  // The hardware has a load sharing circuit to allow simultaneous charge and power
  // With USB(5V) connected the voltage reading will be approx. VBUS - Diode Drop = 4.65V
  // If the average voltage is greater than 4.3V, show ligtning on the display
  if(batteryVolts > 4.3)
  {
    spr.fillRoundRect(x + 2, y + 3, 24, 10, 2, TH.batt_charge);
    spr.drawLine(x + 9 + 8, y + 1, x + 9 + 6, y + 1 + 5, TH.bg);
    spr.drawLine(x + 9 + 6, y + 1 + 5, x + 9 + 10, y + 1 + 5, TH.bg);
    spr.drawLine(x + 9 + 11, y + 1 + 6, x + 9 + 4, y + 1 + 13, TH.bg);
    spr.drawLine(x + 9 + 2, y + 1 + 13, x + 9 + 4, y + 1 + 8, TH.bg);
    spr.drawLine(x + 9 + 4, y + 1 + 8, x + 9 + 0, y + 1 + 8, TH.bg);
    spr.drawLine(x + 9 - 1, y + 1 + 7, x + 9 + 6, y + 1 + 0, TH.bg);
    spr.fillTriangle(x + 9 + 7, y + 1, x + 9 + 4, y + 1 + 6, x + 9, y + 1 + 7, TH.batt_icon);
    spr.fillTriangle(x + 9 + 5, y + 1 + 6, x + 9 + 10, y + 1 + 6, x + 9 + 3, y + 1 + 13, TH.batt_icon);
    spr.fillRect(x + 9 + 1, y + 1 + 6, 9, 2, TH.batt_icon);
    spr.drawPixel(x + 9 + 3, y + 1 + 12, TH.batt_icon);
    return false;
  }
  else
  {
    char voltage[8];
    uint16_t color;
    int level;

    // Text representation of the voltage
    sprintf(voltage, "%.02fV", batteryVolts);

    // Battery bar color and width
    switch(batteryState)
    {
      case 0:
        color = TH.batt_low;
        level = 6;
        break;
      case 1:
        color = TH.batt_full;
        level = 12;
        break;
      case 2:
        color = TH.batt_full;
        level = 18;
        break;
      case 3:
      default:
        color = TH.batt_full;
        level = 24;
        break;
    }

    spr.fillRoundRect(x + 2, y + 3, level, 10, 2, color);
    spr.drawString(voltage, x - 3, y, FONT_SMALL);
    return true;
  }
}
