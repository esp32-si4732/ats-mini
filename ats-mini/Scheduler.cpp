#include "Common.h"
#include "Utils.h"
#include "Menu.h"
#include "Storage.h"
#include "Scheduler.h"

extern long elapsedRSSI;
extern long lastRDSCheck;
extern long lastScheduleCheck;
extern long lastNTPCheck;
extern uint32_t background_timer;

bool runScheduler(uint32_t currentTime)
{
  bool needRedraw = false;

  if((currentTime - elapsedRSSI) > MIN_ELAPSED_RSSI_TIME)
  {
    needRedraw |= processRssiSnr();
    elapsedRSSI = currentTime;
  }

  if((currentTime - lastRDSCheck) > RDS_CHECK_TIME)
  {
    needRedraw |= (radioState.mode == FM) && (snr >= 12) && checkRds();
    lastRDSCheck = currentTime;
  }

  if((currentTime - lastScheduleCheck) > SCHEDULE_CHECK_TIME)
  {
    needRedraw |= identifyFrequency(radioState.frequency + radioState.bfo / 1000, true);
    lastScheduleCheck = currentTime;
  }

  if((currentTime - lastNTPCheck) > NTP_CHECK_TIME)
  {
    needRedraw |= ntpSyncTime();
    lastNTPCheck = currentTime;
  }

  prefsTickTime();
  netTickTime();
  needRedraw |= clockTickTime();

  if(needRedraw) background_timer = currentTime;
  if((currentTime - background_timer) > BACKGROUND_REFRESH_TIME)
  {
    if(radioState.cmd == CMD_NONE) needRedraw = true;
    background_timer = currentTime;
  }

  return needRedraw;
}
