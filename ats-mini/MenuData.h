#ifndef MENU_DATA_H
#define MENU_DATA_H

#include "Common.h"

// Step data type
typedef struct
{
  int step;         // Step
  const char *desc; // Step description
  uint8_t spacing;  // Seek spacing
} Step;

// Bandwidth data type
typedef struct
{
  uint8_t idx;      // SI473X device bandwidth index
  const char *desc; // Bandwidth description
} Bandwidth;

// RDS mode data type
typedef struct
{
  uint8_t mode;     // Combination of RDS_* values
  const char *desc; // Mode description
} RDSMode;

// Band definitions
extern Band bands[];

// Memory
extern Memory memories[];

// Menu and settings text
extern const char *menu[];
extern const char *settings[];
extern const char *bandModeDesc[];
extern const char *sleepModeDesc[];
extern const char *uiLayoutDesc[];
extern const char *usbModeDesc[];
extern const char *bleModeDesc[];
extern const char *wifiModeDesc[];

// Step tables
extern const Step *steps[];

// Bandwidth tables
extern const Bandwidth *bandwidths[];

// Mode data tables
extern const RDSMode rdsMode[];
extern const UTCOffset utcOffsets[];
extern const FMRegion fmRegions[];

#endif
