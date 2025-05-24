#ifndef THEMES_H
#define THEMES_H

#include "Common.h"

// This is our current theme
#define TH (theme[themeIdx])

typedef struct __attribute__ ((packed))
{
  const char *name;
  uint16_t bg;
  uint16_t text;
  uint16_t text_muted;
  uint16_t text_warn;

  uint16_t smeter_icon;
  uint16_t smeter_bar;
  uint16_t smeter_bar_plus;
  uint16_t smeter_bar_empty;

  uint16_t save_icon;

  uint16_t stereo_icon;

  uint16_t wifi_icon;
  uint16_t wifi_icon_conn;

  uint16_t batt_voltage;
  uint16_t batt_border;
  uint16_t batt_full;
  uint16_t batt_low;
  uint16_t batt_charge;
  uint16_t batt_icon;

  uint16_t band_text;

  uint16_t mode_text;
  uint16_t mode_border;

  uint16_t box_bg;
  uint16_t box_border;
  uint16_t box_text;
  uint16_t box_off_bg;
  uint16_t box_off_text;

  uint16_t menu_bg;
  uint16_t menu_border;
  uint16_t menu_hdr;
  uint16_t menu_item;
  uint16_t menu_hl_bg;
  uint16_t menu_hl_text;
  uint16_t menu_param;

  uint16_t freq_text;
  uint16_t funit_text;
  uint16_t freq_hl;
  uint16_t freq_hl_sel;

  uint16_t rds_text;

  uint16_t scale_text;
  uint16_t scale_pointer;
  uint16_t scale_line;
} ColorTheme;

extern uint8_t themeIdx;
int getTotalThemes();

extern ColorTheme theme[];
void setColorTheme();
void getColorTheme();
bool switchThemeEditor(int8_t state = 2);

#endif // THEMES_H
