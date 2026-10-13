#include "Common.h"
#include "Storage.h"
#include "Utils.h"
#include "Menu.h"
#include "TcpMode.h"
#include "Web.h"

#include <atomic>
#include <WiFi.h>
#include <esp_sntp.h>
#include <esp_wifi.h>
#include <ESPmDNS.h>

#define CONNECT_TIME  3000  // Time of inactivity to start connecting WiFi
#define WIFI_TOTAL_TIMEOUT  30000
#define WIFI_ATTEMPT_TIMEOUT  5000
#define WIFI_DISCOVERY_TIMEOUT  15000 // Leave time to connect after probing hidden APs
#define NTP_TIMEOUT  15000

#ifndef WIFI_POWER_LEVEL
#define WIFI_POWER_LEVEL WIFI_POWER_17dBm
#endif

enum class NetState : uint8_t { Idle, Scanning, Probing, Connecting, Retry, Online };
static NetState netState = NetState::Idle;
static uint8_t activeNetMode = NET_OFF;
static uint32_t netStarted, attemptStarted;
static std::atomic<bool> connectRequested{false};
static std::atomic<bool> timeSynchronized{false};
// Retain NTP precedence over RDS even if the user resets clock validity.
static bool ntpHasTime = false;

// Keep the strongest identified AP for each of the three saved networks.
struct WiFiNetwork
{
  String ssid, password;
  int32_t rssi;
  int32_t channel;
  uint8_t bssid[6];
  bool tried;
  bool open;
};
static WiFiNetwork wifiNetworks[3];
// Scan results stay alive while hidden APs are identified using saved credentials.
static int probeAP = 0;
static uint8_t probeNetwork = 0;

//
// Access Point (AP) mode settings
//
static const char *apSSID    = RECEIVER_NAME;
static const char *apPWD     = 0;       // No password
static const int   apChannel = 10;      // WiFi channel number (1..13)
static const bool  apHideMe  = false;   // TRUE: disable SSID broadcast
static const int   apClients = 3;       // Maximum simultaneous connected clients

static bool itIsTimeToWiFi = false; // TRUE: Need to connect to WiFi
static uint32_t connectTime = 0;

static bool wifiScanHidden = false;

static void wifiInitAP();
static void wifiStartScan();
static void wifiTryNext();
static void wifiProbeNext();
static void wifiRememberAP(WiFiNetwork &network, int index);
static void wifiConnected(uint32_t now);
static void wifiConnectionFailed();
static void wifiRegisterPowerLevelCallback();
static void wifiPowerLevelOnEvent(WiFiEvent_t event);

//
// Delayed WiFi connection
//
void netSetScanHidden(bool enabled)
{
  wifiScanHidden = enabled;
}

void netRequestConnect()
{
  // The web callback runs on another task. The main loop owns the deadline.
  connectRequested.store(true);
}

bool netTickTime()
{
  bool changed = false;
  uint32_t now = millis();

  if(connectRequested.exchange(false))
  {
    connectTime = now;
    itIsTimeToWiFi = true;
  }
  if(itIsTimeToWiFi && now - connectTime > CONNECT_TIME)
  {
    netInit(wifiModeIdx);
    now = millis();
  }

  if(timeSynchronized.exchange(false))
  {
    // SNTP has already set system time. Only the main task touches receiver state.
    ntpHasTime = true;
    clockTimeSynchronized();
    changed = true;
    if(netState == NetState::Online && activeNetMode == NET_SYNC)
    {
      netStop();
      statusShow("Time synchronized");
    }
  }

  // Accept a completed connection before checking its overall timeout.
  // Probe associations are handled separately and do not start network services.
  if(netState == NetState::Scanning || netState == NetState::Connecting || netState == NetState::Retry)
  {
    if(WiFi.status() == WL_CONNECTED)
    {
      wifiConnected(now);
      return true;
    }
    if(now - netStarted >= WIFI_TOTAL_TIMEOUT)
    {
      wifiConnectionFailed();
      return changed;
    }
  }

  switch(netState)
  {
    case NetState::Idle:
      break;

    case NetState::Scanning:
    {
      int count = WiFi.scanComplete();
      if(count != WIFI_SCAN_RUNNING)
      {
        for(auto &network : wifiNetworks)
        {
          network.rssi = INT32_MIN;
          network.channel = 0;
          network.tried = false;
          network.open = false;
          for(int i = 0; i < count; ++i)
            if(network.ssid.length() && WiFi.SSID(i) == network.ssid && WiFi.RSSI(i) > network.rssi)
              wifiRememberAP(network, i);
        }
        probeAP = probeNetwork = 0;
        wifiProbeNext();
      }
      break;
    }

    case NetState::Probing:
    {
      // Association is enough to identify an AP; do not wait for DHCP during probes.
      bool connected = WiFi.STA.connected();
      bool discoveryExpired = now - netStarted >= WIFI_DISCOVERY_TIMEOUT;
      if(connected || discoveryExpired || now - attemptStarted >= WIFI_ATTEMPT_TIMEOUT)
      {
        if(connected)
        {
          wifiRememberAP(wifiNetworks[probeNetwork], probeAP);
          ++probeAP;
          probeNetwork = 0;
        }
        else
          ++probeNetwork;
        WiFi.disconnect();
        wifiProbeNext();
      }
      break;
    }

    case NetState::Connecting:
      if(now - attemptStarted >= WIFI_ATTEMPT_TIMEOUT)
      {
        WiFi.disconnect();
        wifiTryNext();
      }
      break;

    case NetState::Retry:
      if(now - attemptStarted >= 1000) wifiStartScan();
      break;

    case NetState::Online:
      if(activeNetMode == NET_SYNC && now - attemptStarted >= NTP_TIMEOUT)
      {
        netStop();
        statusShow("NTP sync failed");
      }
      break;
  }
  return changed;
}

//
// Get current connection status
// (-1 - not connected, 0 - disabled, 1 - connected, 2 - connected to network)
//
int8_t getWiFiStatus()
{
  wifi_mode_t mode = WiFi.getMode();

  switch(mode)
  {
    case WIFI_MODE_NULL:
      return(0);
    case WIFI_AP:
      return(WiFi.softAPgetStationNum()? 1 : -1);
    case WIFI_STA:
      return(WiFi.status()==WL_CONNECTED? 2 : -1);
    case WIFI_AP_STA:
      return((WiFi.status()==WL_CONNECTED)? 2 : WiFi.softAPgetStationNum()? 1 : -1);
    default:
      return(-1);
  }
}

char *getWiFiIPAddress()
{
  static char ip[16];
  return strcpy(ip, WiFi.status()==WL_CONNECTED ? WiFi.localIP().toString().c_str() : "");
}

//
// Stop WiFi hardware
//
void netStop()
{
  // Online owns the SNTP client, including while waiting for the first reply.
  if(netState == NetState::Online) esp_sntp_stop();
  netState = NetState::Idle;
  itIsTimeToWiFi = false;
  connectRequested.store(false);
  timeSynchronized.store(false);
  WiFi.setAutoReconnect(false);
  esp_wifi_scan_stop();
  tcpStop();
  MDNS.end();
  // Disabling WiFi stops both interfaces and any pending connection.
  WiFi.mode(WIFI_MODE_NULL);
  WiFi.scanDelete();
  statusShow(nullptr);
}

//
// Initialize WiFi network and services
//
void netInit(uint8_t netMode)
{
  // Always disable WiFi first
  netStop();
  wifiRegisterPowerLevelCallback();

  if(netMode == NET_OFF) return;
  wifi_mode_t mode = WIFI_STA;
  if(netMode == NET_AP_ONLY) mode = WIFI_AP;
  else if(netMode == NET_AP_CONNECT) mode = WIFI_AP_STA;
  WiFi.mode(mode);
  if(netMode == NET_AP_ONLY || netMode == NET_AP_CONNECT) wifiInitAP();

  activeNetMode = netMode;

  // Load saved station credentials and scan preferences.
  prefs.begin("network", true, STORAGE_PARTITION);
  wifiScanHidden = prefs.getBool("wifiscanhidden", false);
  for(int i = 0; i < 3; ++i)
  {
    char key[16];
    snprintf(key, sizeof(key), "wifissid%d", i + 1);
    wifiNetworks[i].ssid = prefs.getString(key, "");
    snprintf(key, sizeof(key), "wifipass%d", i + 1);
    wifiNetworks[i].password = prefs.getString(key, "");
  }
  prefs.end();

  if(netMode != NET_SYNC)
  {
    webInit();
    MDNS.begin("atsmini");
    MDNS.addService("http", "tcp", 80);
  }

  if(netMode > NET_AP_ONLY)
  {
    if(!wifiNetworks[0].ssid.length() && !wifiNetworks[1].ssid.length() && !wifiNetworks[2].ssid.length())
    {
      wifiConnectionFailed();
      return;
    }
    netStarted = millis();
    statusShow("Connecting to WiFi network...", nullptr, 0);
    wifiStartScan();
  }
  else
    statusShow("Use Access Point " RECEIVER_NAME, "IP : 10.1.1.1 or atsmini.local");
}

bool ntpIsAvailable()
{
  return ntpHasTime;
}

static void wifiRegisterPowerLevelCallback()
{
  static bool registered = false;

  if(registered) return;

  WiFi.onEvent(wifiPowerLevelOnEvent, ARDUINO_EVENT_WIFI_AP_START);
  WiFi.onEvent(wifiPowerLevelOnEvent, ARDUINO_EVENT_WIFI_STA_START);
  registered = true;
}

static void wifiPowerLevelOnEvent(WiFiEvent_t event)
{
  (void)event;
  WiFi.setTxPower(WIFI_POWER_LEVEL);
}

//
// Initialize WiFi access point (AP)
//
static void wifiInitAP()
{
  // These are our own access point (AP) addresses
  IPAddress ip(10, 1, 1, 1);
  IPAddress gateway(10, 1, 1, 1);
  IPAddress subnet(255, 255, 255, 0);

  // Start as access point (AP)
  WiFi.softAP(apSSID, apPWD, apChannel, apHideMe, apClients);
  WiFi.softAPConfig(ip, gateway, subnet);
}

//
// Connect to a WiFi network
//
static void wifiStartScan()
{
  netState = NetState::Scanning;
  WiFi.scanNetworks(true, wifiScanHidden);
}

static void wifiRememberAP(WiFiNetwork &network, int index)
{
  network.rssi = WiFi.RSSI(index);
  network.channel = WiFi.channel(index);
  network.open = WiFi.encryptionType(index) == WIFI_AUTH_OPEN;
  memcpy(network.bssid, WiFi.BSSID(index), sizeof(network.bssid));
}

static void wifiProbeNext()
{
  int count = WiFi.scanComplete();
  while(wifiScanHidden && probeAP < count && millis() - netStarted < WIFI_DISCOVERY_TIMEOUT)
  {
    if(!WiFi.SSID(probeAP).length())
    {
      while(probeNetwork < ITEM_COUNT(wifiNetworks))
      {
        auto &network = wifiNetworks[probeNetwork];
        if(!network.ssid.length() || WiFi.RSSI(probeAP) <= network.rssi)
        {
          ++probeNetwork;
          continue;
        }
        // A successful association identifies this anonymous BSSID before ranking it.
        netState = NetState::Probing;
        attemptStarted = millis();
        const char *password = WiFi.encryptionType(probeAP) == WIFI_AUTH_OPEN || !network.password.length() ?
          nullptr : network.password.c_str();
        WiFi.begin(network.ssid.c_str(), password, WiFi.channel(probeAP), WiFi.BSSID(probeAP));
        return;
      }
    }
    ++probeAP;
    probeNetwork = 0;
  }
  WiFi.scanDelete();
  wifiTryNext();
}

static void wifiTryNext()
{
  WiFiNetwork *best = nullptr;
  for(auto &network : wifiNetworks)
    if(network.ssid.length() && !network.tried && network.channel &&
       (!best || network.rssi > best->rssi))
      best = &network;

  if(!best)
  {
    netState = NetState::Retry;
    attemptStarted = millis();
    return;
  }

  best->tried = true;
  attemptStarted = millis();
  netState = NetState::Connecting;
  const char *password = best->open || !best->password.length() ? nullptr : best->password.c_str();
  WiFi.begin(best->ssid.c_str(), password, best->channel, best->bssid);
}

static void wifiConnected(uint32_t now)
{
  WiFi.scanDelete();
  WiFi.setAutoReconnect(true);
  timeSynchronized.store(false);
  esp_sntp_set_time_sync_notification_cb([](struct timeval *) {
    timeSynchronized.store(true);
  });
  esp_sntp_set_sync_interval(5 * 60 * 1000);
  configTime(0, 0, "pool.ntp.org");
  attemptStarted = now;
  netState = NetState::Online;
  if(activeNetMode == NET_SYNC)
    statusShow("Synchronizing time...", nullptr, 0);
  else
    statusShow(("Connected to WiFi network (" + WiFi.SSID() + ")").c_str(),
               ("IP : " + WiFi.localIP().toString() + " or atsmini.local").c_str());
}

static void wifiConnectionFailed()
{
  if(activeNetMode == NET_SYNC)
    netStop();
  else
  {
    netState = NetState::Idle;
    esp_wifi_scan_stop();
    WiFi.disconnect();
    WiFi.scanDelete();
  }
  if(activeNetMode == NET_AP_CONNECT)
    statusShow("No WiFi connection", "Use Access Point " RECEIVER_NAME " at 10.1.1.1");
  else
    statusShow("No WiFi connection");
}
