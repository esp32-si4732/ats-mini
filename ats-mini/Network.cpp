#include "Common.h"
#include "WiFiManager.h"
#include "Storage.h"
#include "Utils.h"
#include "WebServer.h"

#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// NTP Client to get time
WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP, "pool.ntp.org");

//
// Initialize WiFi network and NTP and web services
//
void netInit(uint8_t netMode, bool showStatus)
{
  // Initialize WiFi connection (AP, station, or both)
  bool connected = wifiInitConnection(netMode, showStatus);

  // NTP time updates will happen every 5 minutes
  if(connected)
  {
    ntpClient.setUpdateInterval(5*60*1000);

    // Get NTP time from the network
    clockReset();
    for(int j=0 ; j<10 ; j++)
      if(ntpSyncTime()) break; else delay(500);
  }

  // If only connected to sync time, drop the connection
  if(netMode==NET_SYNC)
  {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_MODE_NULL);
  }
  else
  {
    // Initialize web server for remote configuration
    webInit();
  }
}

//
// Returns TRUE if NTP time is available
//
bool ntpIsAvailable()
{
  return(ntpClient.isTimeSet());
}

//
// Update NTP time and synchronize clock with NTP time
//
bool ntpSyncTime()
{
  if(getWiFiStatus() == 2)
  {
    ntpClient.update();

    if(ntpClient.isTimeSet())
      return(clockSet(
        ntpClient.getHours(),
        ntpClient.getMinutes(),
        ntpClient.getSeconds()
      ));
  }
  return(false);
}
