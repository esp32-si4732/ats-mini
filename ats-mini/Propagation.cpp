#include "Common.h"
#include "Utils.h"
#include "Draw.h"
#include "Themes.h"
#include "Menu.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

struct PropData
{
  bool valid;
  float sfi;
  float kp;
  char rating10m[16];
  char rating20m[16];
  char rating40m[16];
  char rating80m[16];
};

static PropData propData = { false, 0, 0, "--", "--", "--", "--" };
static int propagationSelection = 0;

static int extractJsonFloat(String& json, const char *key)
{
  int keyPos = json.indexOf(key);
  if(keyPos == -1) return 0;

  int colonPos = json.indexOf(':', keyPos);
  if(colonPos == -1) return 0;

  int valStart = colonPos + 1;
  while(valStart < json.length() && (json[valStart] == ' ' || json[valStart] == '\t')) valStart++;

  int valEnd = valStart;
  while(valEnd < json.length() && (isdigit(json[valEnd]) || json[valEnd] == '.' || json[valEnd] == '-')) valEnd++;

  return (int)(json.substring(valStart, valEnd).toFloat() * 10.0f);
}

static int getLocalHour()
{
  uint8_t h, m;
  int localMinutes = 12 * 60;

  if(clockGetHM(&h, &m))
  {
    localMinutes = (int)h * 60 + m + getCurrentUTCOffset() * 15;
    localMinutes = localMinutes < 0 ? localMinutes + 24 * 60 : localMinutes;
    localMinutes = localMinutes % (24 * 60);
  }

  return localMinutes / 60;
}

static int ratingToScore(const char *rating)
{
  if(strcmp(rating, "Good") == 0) return 3;
  if(strcmp(rating, "Fair") == 0) return 2;
  if(strcmp(rating, "Poor") == 0) return 1;
  return 2;
}

static const char *scoreToLabel(int score)
{
  if(score >= 3) return "Good";
  if(score == 2) return "Fair";
  return "Poor";
}

static int heuristicBandScore(uint32_t freqHz)
{
  uint32_t freqKHz = freqHz / 1000;
  int hour = getLocalHour();

  if(hour >= 6 && hour < 9)
  {
    if(freqKHz >= 12000 && freqKHz <= 22000) return 3;
    if(freqKHz >= 5000 && freqKHz < 12000) return 2;
    return 1;
  }

  if(hour >= 9 && hour < 17)
  {
    if(freqKHz >= 14000 && freqKHz <= 30000) return 3;
    if(freqKHz >= 7000 && freqKHz < 14000) return 2;
    return 1;
  }

  if(hour >= 17 && hour < 21)
  {
    if(freqKHz >= 7000 && freqKHz <= 18000) return 3;
    if(freqKHz >= 3000 && freqKHz < 7000) return 2;
    return 1;
  }

  if(freqKHz >= 3000 && freqKHz <= 10000) return 3;
  if(freqKHz >= 1000 && freqKHz < 3000) return 2;
  return 1;
}

bool propagationDataAvailable()
{
  return propData.valid;
}

int propagationBandScore(uint32_t freqHz)
{
  uint32_t freqKHz = freqHz / 1000;

  if(!propData.valid)
    return heuristicBandScore(freqHz);

  if(freqKHz >= 24000) return ratingToScore(propData.rating10m);
  if(freqKHz >= 14000) return ratingToScore(propData.rating20m);
  if(freqKHz >= 5000) return ratingToScore(propData.rating40m);
  return ratingToScore(propData.rating80m);
}

const char *propagationBandLabel(uint32_t freqHz)
{
  return scoreToLabel(propagationBandScore(freqHz));
}

void propagationMoveSelection(int delta)
{
  int count = getUtilityRecommendationCount();
  if(count <= 0) return;

  propagationSelection += delta;
  propagationSelection = propagationSelection >= count ? propagationSelection % count : propagationSelection;
  propagationSelection = propagationSelection < 0 ? count - ((-propagationSelection) % count) : propagationSelection;
  if(propagationSelection == count) propagationSelection = 0;
}

void propagationResetSelection()
{
  propagationSelection = 0;
}

const UtilFreq *propagationGetSelectedEntry()
{
  return getUtilityRecommendation(propagationSelection);
}

void updatePropagationData()
{
  if(getWiFiStatus() != 2) return;
  if(propData.valid) return;

  WiFiClientSecure *client = new WiFiClientSecure;
  if(client)
  {
    client->setInsecure();
    HTTPClient https;
    https.setTimeout(3000);

    if(https.begin(*client, "https://wspr.hb9vqq.ch/api/dx.json"))
    {
      int httpCode = https.GET();
      if(httpCode == HTTP_CODE_OK)
      {
        String payload = https.getString();

        propData.sfi = extractJsonFloat(payload, "\"sfi\"") / 10.0f;
        propData.kp  = extractJsonFloat(payload, "\"kp\"") / 10.0f;

        auto getRating = [&](const char *band) {
          int bandPos = payload.indexOf(band);
          if(bandPos == -1) return String("--");
          int ratePos = payload.indexOf("\"rating\"", bandPos);
          if(ratePos == -1) return String("--");
          int q1 = payload.indexOf('"', ratePos + 8);
          int q2 = payload.indexOf('"', q1 + 1);
          if(q1 != -1 && q2 != -1) return payload.substring(q1 + 1, q2);
          return String("--");
        };

        strncpy(propData.rating10m, getRating("\"10m\"").c_str(), 15);
        strncpy(propData.rating20m, getRating("\"20m\"").c_str(), 15);
        strncpy(propData.rating40m, getRating("\"40m\"").c_str(), 15);
        strncpy(propData.rating80m, getRating("\"80m\"").c_str(), 15);
        propData.rating10m[15] = '\0';
        propData.rating20m[15] = '\0';
        propData.rating40m[15] = '\0';
        propData.rating80m[15] = '\0';
        propData.valid = true;
      }
      https.end();
    }
    delete client;
  }
}

void drawPropagation()
{
  spr.fillSprite(TH.bg);
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TH.menu_hdr);
  spr.fillSmoothRoundRect(10, 10, 300, 150, 4, TH.menu_border);
  spr.fillSmoothRoundRect(12, 12, 296, 146, 4, TH.menu_bg);
  spr.drawString(propData.valid ? "Propagation & Listening" : "Propagation Guide", 160, 20, 2);
  spr.drawLine(10, 42, 310, 42, TH.menu_border);

  char buf[32];
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TH.menu_item);

  if(propData.valid)
  {
    sprintf(buf, "SFI: %.0f", propData.sfi);
    spr.drawString(buf, 24, 54, 2);
    sprintf(buf, "Kp: %.1f", propData.kp);
    spr.drawString(buf, 110, 54, 2);
    sprintf(buf, "10m %s", propData.rating10m);
    spr.drawString(buf, 170, 54, 2);
    sprintf(buf, "20m %s", propData.rating20m);
    spr.drawString(buf, 24, 72, 2);
    sprintf(buf, "40m %s", propData.rating40m);
    spr.drawString(buf, 110, 72, 2);
    sprintf(buf, "80m %s", propData.rating80m);
    spr.drawString(buf, 200, 72, 2);
  }
  else
  {
    spr.drawString("Offline heuristic active", 24, 54, 2);
    spr.drawString("Band labels adapt to local time", 24, 72, 2);
  }

  const UtilFreq *entry = propagationGetSelectedEntry();

  spr.drawLine(24, 92, 296, 92, TH.menu_border);
  spr.setTextColor(TH.band_text);
  spr.drawString("Listen now", 24, 100, 2);

  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TH.text);
  spr.drawString(entry->name, 160, 116, 2);

  sprintf(buf, "%.3f %s", entry->freq / 1000000.0f, bandModeDesc[entry->mode]);
  spr.setTextColor(TH.menu_param);
  spr.drawString(buf, 160, 134, 2);

  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TH.menu_item);
  spr.drawString(entry->cat, 160, 148, 2);

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TH.rds_text);
  spr.drawString(entry->note, 24, 162, 2);

  spr.pushSprite(0, 0);
}
