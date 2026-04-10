#include "Common.h"
#include "Utils.h"
#include "Draw.h"
#include "Menu.h"
#include "Themes.h"

extern int utilIdx;

void drawUtility()
{
  spr.fillSprite(TH.bg);
  spr.fillSmoothRoundRect(10, 10, 300, 150, 4, TH.menu_border);
  spr.fillSmoothRoundRect(12, 12, 296, 146, 4, TH.menu_bg);

  utilitySyncSelection(&utilIdx);
  const UtilFreq *u = getUtilityVisibleData(utilIdx);

  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TH.menu_hdr);
  spr.drawString("Utility DB", 160, 20, 2);
  spr.drawLine(10, 42, 310, 42, TH.menu_border);

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TH.menu_item);
  spr.drawString("View", 24, 50, 2);
  spr.drawString(getUtilityViewLabel(), 58, 50, 2);
  spr.drawString(getUtilityFilterLabel(), 92, 50, 2);

  char counter[24];
  sprintf(counter, "%d/%d", utilIdx + 1, getUtilityVisibleCount());
  spr.setTextDatum(TR_DATUM);
  spr.drawString(counter, 296, 50, 2);

  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TH.text);
  spr.drawString(u->name, 160, 78, 2);

  char freqStr[32];
  if(u->freq >= 10000000)
    sprintf(freqStr, "%.3f MHz  %s", u->freq / 1000000.0f, bandModeDesc[u->mode]);
  else
    sprintf(freqStr, "%lu kHz  %s", u->freq / 1000, bandModeDesc[u->mode]);

  spr.setTextColor(TH.menu_param);
  spr.drawString(freqStr, 160, 98, 2);

  spr.setTextColor(TH.band_text);
  spr.drawString(u->cat, 160, 116, 2);

  spr.setTextColor(TH.rds_text);
  spr.drawString(u->note, 160, 134, 2);

  spr.setTextColor(TH.text_muted);
  spr.drawString("Turn=tune  Short=view  Long=tune", 160, 154, 2);

  spr.pushSprite(0, 0);
}
