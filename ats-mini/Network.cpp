#include "Common.h"
#include "Storage.h"
#include "Themes.h"
#include "Utils.h"
#include "Menu.h"
#include "Draw.h"

#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiUdp.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <NTPClient.h>
#include <ESPmDNS.h>

#define CONNECT_TIME  3000  // Time of inactivity to start connecting WiFi

WiFiMulti wifiMulti;

//
// Access Point (AP) mode settings
//
static const char *apSSID    = RECEIVER_NAME;
static const char *apPWD     = 0;       // No password
static const int   apChannel = 10;      // WiFi channel number (1..13)
static const bool  apHideMe  = false;   // TRUE: disable SSID broadcast
static const int   apClients = 3;       // Maximum simultaneous connected clients

static uint16_t ajaxInterval = 2500;

static bool itIsTimeToWiFi = false; // TRUE: Need to connect to WiFi
static uint32_t connectTime = millis();

// Settings
String loginUsername = "";
String loginPassword = "";

// AsyncWebServer object on port 80
AsyncWebServer server(80);

// NTP Client to get time
WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP, "pool.ntp.org");

static bool wifiInitAP();
static bool wifiConnect();
static void webInit();

static void webSetConfig(AsyncWebServerRequest *request);
static void webSetControl(AsyncWebServerRequest *request);
static void webImportBackup(AsyncWebServerRequest *request);
static String webStatusJson();
static String webExportMemories();
static String webExportSettings();
static bool webApplyBackup(const String &text);

static const String webInputField(const String &name, const String &value, bool pass = false);
static const String webStyleSheet();
static const String webPage(const String &body);
static const String webUtcOffsetSelector();
static const String webThemeSelector();
static const String webClockSourceSelector();
static const String webRadioPage();
static const String webMemoryPage();
static const String webConfigPage();

//
// Delayed WiFi connection
//
void netRequestConnect()
{
  connectTime = millis();
  itIsTimeToWiFi = true;
}

void netTickTime()
{
  // Connect to WiFi if requested
  if(itIsTimeToWiFi && ((millis() - connectTime) > CONNECT_TIME))
  {
    netInit(wifiModeIdx);
    connectTime = millis();
    itIsTimeToWiFi = false;
  }
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
  wifi_mode_t mode = WiFi.getMode();

  MDNS.end();

  // If network connection up, shut it down
  if((mode==WIFI_STA) || (mode==WIFI_AP_STA))
    WiFi.disconnect(true);

  // If access point up, shut it down
  if((mode==WIFI_AP) || (mode==WIFI_AP_STA))
    WiFi.softAPdisconnect(true);

  WiFi.mode(WIFI_MODE_NULL);
}

//
// Initialize WiFi network and services
//
void netInit(uint8_t netMode, bool showStatus)
{
  // Always disable WiFi first
  netStop();

  switch(netMode)
  {
    case NET_OFF:
      // Do not initialize WiFi if disabled
      return;
    case NET_AP_ONLY:
      // Start WiFi access point if requested
      WiFi.mode(WIFI_AP);
      // Let user see connection status if successful
      if(wifiInitAP() && showStatus) delay(2000);
      break;
    case NET_AP_CONNECT:
      // Start WiFi access point if requested
      WiFi.mode(WIFI_AP_STA);
      // Let user see connection status if successful
      if(wifiInitAP() && showStatus) delay(2000);
      break;
    default:
      // No access point
      WiFi.mode(WIFI_STA);
      break;
  }

  // Initialize WiFi and try connecting to a network
  if(netMode>NET_AP_ONLY && wifiConnect())
  {
    // Let user see connection status if successful
    if(netMode!=NET_SYNC && showStatus) delay(2000);

    // NTP time updates will happen every 5 minutes
    ntpClient.setUpdateInterval(5*60*1000);

    // Get NTP time from the network if allowed
    if(timeSourceIdx != CLOCK_RDS)
    {
      clockReset();
      for(int j=0 ; j<10 ; j++)
        if(ntpSyncTime()) break; else delay(500);
    }
      
    // Fetch Solar/DX Data once
    updatePropagationData();
  }

  // If only connected to sync...
  if(netMode==NET_SYNC)
  {
    // Drop network connection
    WiFi.disconnect(true);
    WiFi.mode(WIFI_MODE_NULL);
  }
  else
  {
    // Initialize web server for remote configuration
    webInit();

    // Initialize mDNS
    MDNS.begin("atsmini"); // Set the hostname to "atsmini.local"
    MDNS.addService("http", "tcp", 80);
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
  if(timeSourceIdx == CLOCK_RDS) return(false);

  if(WiFi.status()==WL_CONNECTED)
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

//
// Initialize WiFi access point (AP)
//
static bool wifiInitAP()
{
  // These are our own access point (AP) addresses
  IPAddress ip(10, 1, 1, 1);
  IPAddress gateway(10, 1, 1, 1);
  IPAddress subnet(255, 255, 255, 0);

  // Start as access point (AP)
  WiFi.softAP(apSSID, apPWD, apChannel, apHideMe, apClients);
  WiFi.softAPConfig(ip, gateway, subnet);

  drawScreen(
    ("Use Access Point " + String(apSSID)).c_str(),
    ("IP : " + WiFi.softAPIP().toString() + " or atsmini.local").c_str()
  );

  ajaxInterval = 2500;
  return(true);
}

//
// Connect to a WiFi network
//
static bool wifiConnect()
{
  String status = "Connecting to WiFi network...";

  // Clean credentials
  wifiMulti.APlistClean();

  // Get the preferences
  prefs.begin("network", true, STORAGE_PARTITION);
  loginUsername = prefs.getString("loginusername", "");
  loginPassword = prefs.getString("loginpassword", "");

  // Try connecting to known WiFi networks
  for(int j=0 ; (j<3) ; j++)
  {
    char nameSSID[16], namePASS[16];
    sprintf(nameSSID, "wifissid%d", j+1);
    sprintf(namePASS, "wifipass%d", j+1);

    String ssid = prefs.getString(nameSSID, "");
    String password = prefs.getString(namePASS, "");

    if(ssid != "")
      wifiMulti.addAP(ssid.c_str(), password.c_str());
  }

  // Done with preferences
  prefs.end();

  drawScreen(status.c_str());

  // If failed connecting to WiFi network...
  if (wifiMulti.run() != WL_CONNECTED)
  {
    // WiFi connection failed
    drawScreen(status.c_str(), "No WiFi connection");
    // Done
    return(false);
  }
  else
  {
    // WiFi connection succeeded
    drawScreen(
      ("Connected to WiFi network (" + WiFi.SSID() + ")").c_str(),
      ("IP : " + WiFi.localIP().toString() + " or atsmini.local").c_str()
    );
    // Done
    ajaxInterval = 1000;
    return(true);
  }
}

//
// Initialize internal web server
//
static void webInit()
{
  server.on("/", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/html", webRadioPage());
  });

  server.on("/memory", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/html", webMemoryPage());
  });

  server.on("/config", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    if(loginUsername != "" && loginPassword != "")
      if(!request->authenticate(loginUsername.c_str(), loginPassword.c_str()))
        return request->requestAuthentication();
    request->send(200, "text/html", webConfigPage());
  });

  server.onNotFound([] (AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
  });

  // This method saves configuration form contents
  server.on("/setconfig", HTTP_ANY, webSetConfig);
  server.on("/api/import", HTTP_ANY, webImportBackup);
  server.on("/api/export/memory", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/plain", webExportMemories());
  });
  server.on("/api/export/settings", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/plain", webExportSettings());
  });

  // API Control
  server.on("/api/control", HTTP_ANY, webSetControl);
  server.on("/api/status", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    request->send(200, "application/json", webStatusJson());
  });

  // Start web server
  server.begin();
}

void webSetControl(AsyncWebServerRequest *request)
{
  bool changed = false;

  if(request->hasParam("freq"))
  {
    String freq = request->getParam("freq")->value();
    if (currentMode == FM)
        updateFrequency(freq.toFloat() * 100);
    else
        updateFrequency(freq.toInt());
    changed = true;
  }

  if(request->hasParam("vol"))
  {
    String vol = request->getParam("vol")->value();
    volume = constrain(vol.toInt(), 0, 63);
    rx.setVolume(volume);
    changed = true;
  }
  
  if(request->hasParam("band"))
  {
    String val = request->getParam("band")->value();
    selectBand(val.toInt());
    changed = true;
  }

  if(changed)
  {
    prefsRequestSave(SAVE_ALL);
    drawScreen();
  }

  if(request->hasParam("ajax"))
    request->send(200, "application/json", webStatusJson());
  else
    request->redirect("/");
}

void webImportBackup(AsyncWebServerRequest *request)
{
  String payload = "";

  if(request->hasParam("backup", true))
    payload = request->getParam("backup", true)->value();

  webApplyBackup(payload);
  request->redirect("/config");
}

void webSetConfig(AsyncWebServerRequest *request)
{
  uint32_t prefsSave = 0;

  // Start modifying preferences
  prefs.begin("network", false, STORAGE_PARTITION);

  // Save user name and password
  if(request->hasParam("username", true) && request->hasParam("password", true))
  {
    loginUsername = request->getParam("username", true)->value();
    loginPassword = request->getParam("password", true)->value();

    prefs.putString("loginusername", loginUsername);
    prefs.putString("loginpassword", loginPassword);
  }

  // Save SSIDs and their passwords
  bool haveSSID = false;
  for(int j=0 ; j<3 ; j++)
  {
    char nameSSID[16], namePASS[16];

    sprintf(nameSSID, "wifissid%d", j+1);
    sprintf(namePASS, "wifipass%d", j+1);

    if(request->hasParam(nameSSID, true) && request->hasParam(namePASS, true))
    {
      String ssid = request->getParam(nameSSID, true)->value();
      String pass = request->getParam(namePASS, true)->value();
      prefs.putString(nameSSID, ssid);
      prefs.putString(namePASS, pass);
      haveSSID |= ssid != "" && pass != "";
    }
  }

  // Save time zone
  if(request->hasParam("utcoffset", true))
  {
    String utcOffset = request->getParam("utcoffset", true)->value();
    utcOffsetIdx = utcOffset.toInt();
    clockRefreshTime();
    prefsSave |= SAVE_SETTINGS;
  }

  if(request->hasParam("timesource", true))
  {
    String source = request->getParam("timesource", true)->value();
    timeSourceIdx = source.toInt();
    prefsSave |= SAVE_SETTINGS;
  }

  // Save theme
  if(request->hasParam("theme", true))
  {
    String theme = request->getParam("theme", true)->value();
    themeIdx = theme.toInt();
    prefsSave |= SAVE_SETTINGS;
  }

  // Save scroll direction and menu zoom
  scrollDirection = request->hasParam("scroll", true)? -1 : 1;
  zoomMenu        = request->hasParam("zoom", true);
  prefsSave |= SAVE_SETTINGS;

  // Done with the preferences
  prefs.end();

  // Save preferences immediately
  prefsRequestSave(prefsSave, true);

  // Show config page again
  request->redirect("/config");

  // If we are currently in AP mode, and infrastructure mode requested,
  // and there is at least one SSID / PASS pair, request network connection
  if(haveSSID && (wifiModeIdx>NET_AP_ONLY) && (WiFi.status()!=WL_CONNECTED))
    netRequestConnect();
}

static const String webInputField(const String &name, const String &value, bool pass)
{
  String newValue(value);

  newValue.replace("\"", "&quot;");
  newValue.replace("'", "&apos;");

  return(
    "<INPUT TYPE='" + String(pass? "PASSWORD":"TEXT") + "' NAME='" +
    name + "' VALUE='" + newValue + "'>"
  );
}

static String jsonEscape(String value)
{
  value.replace("\\", "\\\\");
  value.replace("\"", "\\\"");
  value.replace("\n", " ");
  value.replace("\r", " ");
  return value;
}

static String webStatusJson()
{
  String ip = WiFi.status()==WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  String ssid = WiFi.status()==WL_CONNECTED ? WiFi.SSID() : String(apSSID);
  String station = String(getStationName());
  if(!station.length()) station = "--";
  String clock = clockGet() ? String(clockGet()) : String("--:--");
  String freq = currentMode == FM ?
    String(currentFrequency / 100.0, 2) + " MHz" :
    String((currentFrequency * 1000 + currentBFO) / 1000.0, 3) + " kHz";
  String freqValue = currentMode == FM ? String(currentFrequency / 100.0, 2) : String(currentFrequency);
  String freqMin = currentMode == FM ? String(getCurrentBand()->minimumFreq / 100.0, 2) : String(getCurrentBand()->minimumFreq);
  String freqMax = currentMode == FM ? String(getCurrentBand()->maximumFreq / 100.0, 2) : String(getCurrentBand()->maximumFreq);
  String freqStep = currentMode == FM ? String(getCurrentStep()->step / 100.0, 2) : String(getCurrentStep()->step);
  static const char *clockSourceDesc[] = { "Auto", "WiFi/NTP", "RDS" };

  String json = "{";
  json += "\"ip\":\"" + jsonEscape(ip) + "\",";
  json += "\"ssid\":\"" + jsonEscape(ssid) + "\",";
  json += "\"freq\":\"" + jsonEscape(freq) + "\",";
  json += "\"freqValue\":\"" + jsonEscape(freqValue) + "\",";
  json += "\"freqMin\":\"" + jsonEscape(freqMin) + "\",";
  json += "\"freqMax\":\"" + jsonEscape(freqMax) + "\",";
  json += "\"freqStep\":\"" + jsonEscape(freqStep) + "\",";
  json += "\"band\":\"" + jsonEscape(String(getCurrentBand()->bandName)) + "\",";
  json += "\"mode\":\"" + jsonEscape(String(bandModeDesc[currentMode])) + "\",";
  json += "\"station\":\"" + jsonEscape(station) + "\",";
  json += "\"clock\":\"" + jsonEscape(clock) + "\",";
  json += "\"clockSource\":\"" + jsonEscape(String(clockSourceDesc[timeSourceIdx])) + "\",";
  json += "\"rssi\":" + String(rssi) + ",";
  json += "\"snr\":" + String(snr) + ",";
  json += "\"volume\":" + String(volume) + ",";
  json += "\"battery\":\"" + String(batteryMonitor(), 2) + " V\",";
  json += "\"wifi\":" + String(getWiFiStatus()) + ",";
  json += "\"version\":\"" + jsonEscape(String(getVersion(true))) + "\"";
  json += "}";
  return json;
}

static String webExportMemories()
{
  String result = "# ATS-Mini memory backup\n";

  for(int i = 0; i < MEMORY_COUNT; i++)
  {
    if(memories[i].freq && memories[i].band < getTotalBands() && memories[i].mode < getTotalModes())
    {
      result += "#" + String(i + 1 < 10 ? "0" : "") + String(i + 1) + ",";
      result += String(bands[memories[i].band].bandName) + ",";
      result += String(memories[i].freq) + ",";
      result += String(bandModeDesc[memories[i].mode]) + "\n";
    }
  }

  return result;
}

static String webExportSettings()
{
  String result = "# ATS-Mini settings backup\n";
  result += "volume=" + String(volume) + "\n";
  result += "wifiMode=" + String(wifiModeIdx) + "\n";
  result += "utcOffset=" + String(utcOffsetIdx) + "\n";
  result += "timeSource=" + String(timeSourceIdx) + "\n";
  result += "theme=" + String(themeIdx) + "\n";
  result += "zoomMenu=" + String(zoomMenu ? 1 : 0) + "\n";
  result += "scrollDir=" + String(scrollDirection < 0 ? -1 : 1) + "\n";
  result += "brightness=" + String(currentBrt) + "\n";
  result += "sleep=" + String(currentSleep) + "\n";
  result += "uiLayout=" + String(uiLayoutIdx) + "\n";
  result += "usbMode=" + String(usbModeIdx) + "\n";
  result += "bleMode=" + String(bleModeIdx) + "\n";
  return result;
}

static bool webApplyBackup(const String &text)
{
  int start = 0;
  bool changed = false;

  while(start < text.length())
  {
    int end = text.indexOf('\n', start);
    if(end == -1) end = text.length();

    String line = text.substring(start, end);
    line.trim();
    start = end + 1;

    if(!line.length()) continue;

    if(line[0] == '#' && line.length() > 1 && isdigit(line[1]))
    {
      int c1 = line.indexOf(',');
      int c2 = line.indexOf(',', c1 + 1);
      int c3 = line.indexOf(',', c2 + 1);
      if(c1 == -1 || c2 == -1 || c3 == -1) continue;

      int slot = line.substring(1, c1).toInt();
      String bandName = line.substring(c1 + 1, c2);
      uint32_t freq = (uint32_t)line.substring(c2 + 1, c3).toInt();
      String modeName = line.substring(c3 + 1);

      if(slot < 1 || slot > MEMORY_COUNT) continue;

      uint8_t band = 0xFF;
      uint8_t mode = 0xFF;

      for(int i = 0; i < getTotalBands(); i++)
        if(bandName == bands[i].bandName) { band = i; break; }
      for(int i = 0; i < getTotalModes(); i++)
        if(modeName == bandModeDesc[i]) { mode = i; break; }

      if(band == 0xFF || mode == 0xFF) continue;

      memories[slot - 1].band = band;
      memories[slot - 1].mode = mode;
      memories[slot - 1].freq = freq;
      changed = true;
      continue;
    }

    if(line[0] == '#') continue;

    int eq = line.indexOf('=');
    if(eq == -1) continue;

    String key = line.substring(0, eq);
    String value = line.substring(eq + 1);
    key.trim();
    value.trim();

    if(key == "volume") volume = constrain(value.toInt(), 0, 63);
    else if(key == "wifiMode") wifiModeIdx = value.toInt();
    else if(key == "utcOffset") utcOffsetIdx = value.toInt();
    else if(key == "timeSource") timeSourceIdx = value.toInt();
    else if(key == "theme") themeIdx = value.toInt();
    else if(key == "zoomMenu") zoomMenu = value.toInt() != 0;
    else if(key == "scrollDir") scrollDirection = value.toInt() < 0 ? -1 : 1;
    else if(key == "brightness") currentBrt = value.toInt();
    else if(key == "sleep") currentSleep = value.toInt();
    else if(key == "uiLayout") uiLayoutIdx = value.toInt();
    else if(key == "usbMode") usbModeIdx = value.toInt();
    else if(key == "bleMode") bleModeIdx = value.toInt();
    changed = true;
  }

  if(changed)
  {
    rx.setVolume(volume);
    clockRefreshTime();
    prefsRequestSave(SAVE_ALL, true);
    drawScreen();
  }

  return changed;
}

static const String webStyleSheet()
{
  return
"body{margin:0;padding:0;background:#0f1720;color:#e5eef7;font-family:Segoe UI,Arial,sans-serif;}"
".shell{max-width:1100px;margin:0 auto;padding:20px;}"
".topbar{display:flex;justify-content:space-between;align-items:center;gap:12px;flex-wrap:wrap;margin-bottom:18px;}"
".brand{font-size:28px;font-weight:700;letter-spacing:.04em;}"
".nav a{color:#9fd3ff;text-decoration:none;margin-left:14px;}"
".hero{display:grid;grid-template-columns:2fr 1fr;gap:16px;margin-bottom:16px;}"
".panel,.card,.table-card{background:linear-gradient(180deg,#14202c,#101923);border:1px solid #21384d;border-radius:18px;box-shadow:0 12px 30px rgba(0,0,0,.22);}"
".panel{padding:20px;}"
".freq{font-size:42px;font-weight:700;line-height:1.05;color:#f7fbff;}"
".sub{color:#91a9bd;font-size:14px;}"
".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:12px;}"
".card{padding:14px;min-height:82px;}"
".label{display:block;color:#89a2b6;font-size:12px;text-transform:uppercase;letter-spacing:.08em;margin-bottom:6px;}"
".value{font-size:24px;font-weight:700;color:#f4f8fb;}"
".hint{font-size:13px;color:#8fc5f0;}"
".actions{display:grid;grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:16px;margin-top:16px;}"
".table-card{padding:18px;margin-top:18px;}"
"table{width:100%;border-collapse:collapse;}"
"th,td{padding:10px 8px;border-bottom:1px solid #1c3142;text-align:left;}"
"th{color:#9ec4e0;font-size:12px;text-transform:uppercase;letter-spacing:.08em;}"
"input[type=text],input[type=password],select{width:100%;padding:11px 12px;border-radius:10px;border:1px solid #2a4a63;background:#0c141d;color:#edf6ff;box-sizing:border-box;}"
"input[type=submit],button{width:100%;padding:12px 14px;border:none;border-radius:12px;background:linear-gradient(90deg,#1f8fff,#36c2ff);color:#fff;font-weight:700;cursor:pointer;}"
".status-dot{display:inline-block;width:10px;height:10px;border-radius:50%;background:#36c2ff;margin-right:8px;}"
".muted{color:#89a2b6;}"
"@media(max-width:780px){.hero{grid-template-columns:1fr;}.freq{font-size:34px;}}"
;
}

static const String webPage(const String &body)
{
  return
"<!DOCTYPE HTML>"
"<HTML>"
"<HEAD>"
  "<META CHARSET='UTF-8'>"
  "<META NAME='viewport' CONTENT='width=device-width, initial-scale=1.0'>"
  "<TITLE>ATS-Mini Control Panel</TITLE>"
  "<STYLE>" + webStyleSheet() + "</STYLE>"
"</HEAD>"
"<BODY>" + body + "</BODY>"
"</HTML>"
;
}

static const String webUtcOffsetSelector()
{
  String result = "";

  for(int i=0 ; i<getTotalUTCOffsets(); i++)
  {
    char text[64];

    sprintf(text,
      "<OPTION VALUE='%d'%s>%s</OPTION>",
      i, utcOffsetIdx==i? " SELECTED":"",
      utcOffsets[i].desc
    );

    result += text;
  }

  return(result);
}

static const String webThemeSelector()
{
  String result = "";

  for(int i=0 ; i<getTotalThemes(); i++)
  {
    char text[64];

    sprintf(text,
      "<OPTION VALUE='%d'%s>%s</OPTION>",
       i, themeIdx==i? " SELECTED":"", theme[i].name
    );

    result += text;
  }

  return(result);
}

static const String webClockSourceSelector()
{
  static const char *clockSourceDesc[] = { "Auto", "WiFi/NTP", "RDS" };
  String result = "";

  for(int i = 0; i < 3; i++)
  {
    char text[64];
    sprintf(text, "<OPTION VALUE='%d'%s>%s</OPTION>", i, timeSourceIdx == i ? " SELECTED" : "", clockSourceDesc[i]);
    result += text;
  }

  return result;
}

static const String webRadioPage()
{
  String ip = "";
  String ssid = "";
  String freq = currentMode == FM?
    String(currentFrequency / 100.0, 2) + " MHz"
  : String((currentFrequency * 1000 + currentBFO) / 1000.0, 3) + " kHz";
  String freqInputValue = currentMode == FM ?
    String(currentFrequency / 100.0, 2) :
    String(currentFrequency);
  String freqMin = currentMode == FM ?
    String(getCurrentBand()->minimumFreq / 100.0, 2) :
    String(getCurrentBand()->minimumFreq);
  String freqMax = currentMode == FM ?
    String(getCurrentBand()->maximumFreq / 100.0, 2) :
    String(getCurrentBand()->maximumFreq);
  String freqStep = currentMode == FM ?
    String(getCurrentStep()->step / 100.0, 2) :
    String(getCurrentStep()->step);
  String station = String(getStationName());
  if(!station.length()) station = "--";
  String clock = clockGet() ? String(clockGet()) : String("--:--");
  static const char *clockSourceDesc[] = { "Auto", "WiFi/NTP", "RDS" };

  if(WiFi.status()==WL_CONNECTED)
  {
    ip = WiFi.localIP().toString();
    ssid = WiFi.SSID();
  }
  else
  {
    ip = WiFi.softAPIP().toString();
    ssid = String(apSSID);
  }

  return webPage(
"<DIV CLASS='shell'>"
"<DIV CLASS='topbar'>"
  "<DIV CLASS='brand'>ATS-Mini Control Panel</DIV>"
  "<DIV CLASS='nav'><A HREF='/'>Dashboard</A><A HREF='/memory'>Memory</A><A HREF='/config'>Config</A></DIV>"
"</DIV>"
"<DIV CLASS='hero'>"
  "<DIV CLASS='panel'>"
    "<DIV CLASS='sub'><SPAN CLASS='status-dot'></SPAN><SPAN ID='ssid'>" + ssid + "</SPAN> • <SPAN ID='ip'>" + ip + "</SPAN></DIV>"
    "<DIV CLASS='freq' ID='freq'>" + freq + "</DIV>"
    "<DIV CLASS='sub'><SPAN ID='band'>" + String(getCurrentBand()->bandName) + "</SPAN> • <SPAN ID='mode'>" + String(bandModeDesc[currentMode]) + "</SPAN> • Station <SPAN ID='station'>" + station + "</SPAN></DIV>"
    "<DIV CLASS='actions'>"
      "<DIV CLASS='card'><SPAN CLASS='label'>Signal</SPAN><DIV CLASS='value'><SPAN ID='rssi'>" + String(rssi) + "</SPAN> dBuV</DIV><DIV CLASS='hint'>SNR <SPAN ID='snr'>" + String(snr) + "</SPAN> dB</DIV></DIV>"
      "<DIV CLASS='card'><SPAN CLASS='label'>Power</SPAN><DIV CLASS='value' ID='battery'>" + String(batteryMonitor(), 2) + " V</DIV><DIV CLASS='hint'>Volume <SPAN ID='volume'>" + String(volume) + "</SPAN>/63</DIV></DIV>"
      "<DIV CLASS='card'><SPAN CLASS='label'>Clock</SPAN><DIV CLASS='value' ID='clock'>" + clock + "</DIV><DIV CLASS='hint'>Source <SPAN ID='clockSource'>" + String(clockSourceDesc[timeSourceIdx]) + "</SPAN></DIV></DIV>"
    "</DIV>"
  "</DIV>"
  "<DIV CLASS='panel'>"
    "<SPAN CLASS='label'>Receiver</SPAN>"
    "<DIV CLASS='value' style='font-size:20px'>" + String(getVersion(true)) + "</DIV>"
    "<DIV CLASS='hint'>MAC " + String(getMACAddress()) + "</DIV>"
    "<DIV CLASS='hint'>WiFi state " + String(getWiFiStatus()) + "</DIV>"
    "<DIV CLASS='hint' style='margin-top:18px'>Live polling every " + String(ajaxInterval / 1000.0, 1) + "s</DIV>"
  "</DIV>"
"</DIV>"
"<DIV CLASS='actions'>"
  "<DIV CLASS='table-card'>"
    "<H2>Live Control</H2>"
    "<DIV CLASS='label'>Frequency</DIV>"
    "<DIV CLASS='value' style='font-size:18px' ID='freqSliderLabel'>" + freq + "</DIV>"
    "<INPUT TYPE='RANGE' ID='freqSlider' MIN='" + freqMin + "' MAX='" + freqMax + "' STEP='" + freqStep + "' VALUE='" + freqInputValue + "'>"
    "<DIV CLASS='hint'>Range follows current band. For SSB it tunes the carrier in kHz.</DIV>"
    "<DIV STYLE='height:16px'></DIV>"
    "<DIV CLASS='label'>Volume</DIV>"
    "<DIV CLASS='value' style='font-size:18px'><SPAN ID='volSliderLabel'>" + String(volume) + "</SPAN>/63</DIV>"
    "<INPUT TYPE='RANGE' ID='volSlider' MIN='0' MAX='63' STEP='1' VALUE='" + String(volume) + "'>"
    "<DIV STYLE='height:16px'></DIV>"
    "<DIV CLASS='label'>Band Index</DIV>"
    "<FORM ACTION='/api/control' METHOD='GET'>"
      "<INPUT TYPE='TEXT' NAME='band' VALUE='" + String(bandIdx) + "'>"
      "<DIV STYLE='margin-top:12px'><INPUT TYPE='SUBMIT' VALUE='Change Band'></DIV>"
    "</FORM>"
  "</DIV>"
  "<DIV CLASS='table-card'>"
    "<H2>Live Status</H2>"
    "<TABLE>"
      "<TR><TH>Field</TH><TH>Value</TH></TR>"
      "<TR><TD>Clock Source</TD><TD>" + String(clockSourceDesc[timeSourceIdx]) + "</TD></TR>"
      "<TR><TD>Battery</TD><TD>" + String(batteryMonitor(), 2) + " V</TD></TR>"
      "<TR><TD>Station</TD><TD>" + station + "</TD></TR>"
      "<TR><TD>IP / SSID</TD><TD>" + ip + " / " + ssid + "</TD></TR>"
    "</TABLE>"
  "</DIV>"
"</DIV>"
"<SCRIPT>"
"let freqTimer=null;"
"let volTimer=null;"
"function sendLive(params){ fetch('/api/control?ajax=1&'+params).then(r=>r.json()).then(updateStatus).catch(()=>{}); }"
"function formatFreqValue(v,mode){ return mode==='FM' ? Number(v).toFixed(2)+' MHz' : Number(v).toFixed(0)+' kHz'; }"
"function scheduleFreqSend(){ const v=document.getElementById('freqSlider').value; const mode=document.getElementById('mode').textContent; document.getElementById('freqSliderLabel').textContent=formatFreqValue(v,mode); clearTimeout(freqTimer); freqTimer=setTimeout(()=>sendLive('freq='+encodeURIComponent(v)),120); }"
"function scheduleVolSend(){ const v=document.getElementById('volSlider').value; document.getElementById('volSliderLabel').textContent=v; clearTimeout(volTimer); volTimer=setTimeout(()=>sendLive('vol='+encodeURIComponent(v)),80); }"
"async function refreshStatus(){"
"const r=await fetch('/api/status');"
"const s=await r.json();"
"updateStatus(s);"
"}"
"function updateStatus(s){"
"document.getElementById('ip').textContent=s.ip;"
"document.getElementById('ssid').textContent=s.ssid;"
"document.getElementById('freq').textContent=s.freq;"
"document.getElementById('band').textContent=s.band;"
"document.getElementById('mode').textContent=s.mode;"
"document.getElementById('station').textContent=s.station;"
"document.getElementById('rssi').textContent=s.rssi;"
"document.getElementById('snr').textContent=s.snr;"
"document.getElementById('volume').textContent=s.volume;"
"document.getElementById('battery').textContent=s.battery;"
"document.getElementById('clock').textContent=s.clock;"
"document.getElementById('clockSource').textContent=s.clockSource;"
"document.getElementById('freqSlider').min=s.freqMin;"
"document.getElementById('freqSlider').max=s.freqMax;"
"document.getElementById('freqSlider').step=s.freqStep;"
"document.getElementById('freqSlider').value=s.freqValue;"
"document.getElementById('freqSliderLabel').textContent=s.freq;"
"document.getElementById('volSlider').value=s.volume;"
"document.getElementById('volSliderLabel').textContent=s.volume;"
"}"
"document.getElementById('freqSlider').addEventListener('input',scheduleFreqSend);"
"document.getElementById('volSlider').addEventListener('input',scheduleVolSend);"
"setInterval(refreshStatus," + String(ajaxInterval) + ");"
"</SCRIPT>"
"</DIV>"
);
}

static const String webMemoryPage()
{
  String items = "";

  for(int j=0 ; j<MEMORY_COUNT ; j++)
  {
    char text[64];
    sprintf(text, "<TR><TD CLASS='LABEL' WIDTH='10%%'>%02d</TD><TD>", j+1);
    items += text;

    if(!memories[j].freq)
      items += "&nbsp;---&nbsp;</TD></TR>";
    else
    {
      String freq = memories[j].mode == FM?
        String(memories[j].freq / 1000000.0) + "MHz "
      : String(memories[j].freq / 1000.0) + "kHz ";
      items += freq + bandModeDesc[memories[j].mode] + "</TD></TR>";
    }
  }

  return webPage(
"<DIV CLASS='shell'>"
"<DIV CLASS='topbar'>"
  "<DIV CLASS='brand'>ATS-Mini Memory</DIV>"
  "<DIV CLASS='nav'><A HREF='/'>Dashboard</A><A HREF='/memory'>Memory</A><A HREF='/config'>Config</A></DIV>"
"</DIV>"
"<DIV CLASS='table-card'><H2>Saved Favorites</H2><TABLE COLUMNS=2>" + items + "</TABLE></DIV>"
"<DIV CLASS='actions'>"
  "<DIV CLASS='table-card'>"
    "<H2>Export Favorites</H2>"
    "<P CLASS='muted'>Open the plain text export or copy the contents into your notes.</P>"
    "<P><A HREF='/api/export/memory'>Download Memory Backup</A></P>"
    "<TEXTAREA STYLE='width:100%;min-height:180px;background:#0c141d;color:#edf6ff;border:1px solid #2a4a63;border-radius:12px;padding:12px;box-sizing:border-box'>" + webExportMemories() + "</TEXTAREA>"
  "</DIV>"
  "<DIV CLASS='table-card'>"
    "<H2>Restore Favorites</H2>"
    "<FORM ACTION='/api/import' METHOD='POST'>"
      "<TEXTAREA NAME='backup' STYLE='width:100%;min-height:180px;background:#0c141d;color:#edf6ff;border:1px solid #2a4a63;border-radius:12px;padding:12px;box-sizing:border-box' PLACEHOLDER='#01,VHF,107900000,FM'></TEXTAREA>"
      "<DIV STYLE='margin-top:14px'><INPUT TYPE='SUBMIT' VALUE='Restore Backup'></DIV>"
    "</FORM>"
  "</DIV>"
"</DIV>"
"</DIV>"
);
}

const String webConfigPage()
{
  prefs.begin("network", true, STORAGE_PARTITION);
  String ssid1 = prefs.getString("wifissid1", "");
  String pass1 = prefs.getString("wifipass1", "");
  String ssid2 = prefs.getString("wifissid2", "");
  String pass2 = prefs.getString("wifipass2", "");
  String ssid3 = prefs.getString("wifissid3", "");
  String pass3 = prefs.getString("wifipass3", "");
  prefs.end();

  return webPage(
"<DIV CLASS='shell'>"
"<DIV CLASS='topbar'>"
  "<DIV CLASS='brand'>ATS-Mini Config</DIV>"
  "<DIV CLASS='nav'><A HREF='/'>Dashboard</A><A HREF='/memory'>Memory</A><A HREF='/config'>Config</A></DIV>"
"</DIV>"
"<DIV CLASS='actions'>"
"<DIV CLASS='table-card'>"
"<FORM ACTION='/setconfig' METHOD='POST'>"
  "<TABLE COLUMNS=2>"
  "<TR><TH COLSPAN=2 CLASS='HEADING'>WiFi Network 1</TH></TR>"
  "<TR>"
    "<TD CLASS='LABEL'>SSID</TD>"
    "<TD>" + webInputField("wifissid1", ssid1) + "</TD>"
  "</TR>"
  "<TR>"
    "<TD CLASS='LABEL'>Password</TD>"
    "<TD>" + webInputField("wifipass1", pass1, true) + "</TD>"
  "</TR>"
  "<TR><TH COLSPAN=2 CLASS='HEADING'>WiFi Network 2</TH></TR>"
  "<TR>"
    "<TD CLASS='LABEL'>SSID</TD>"
    "<TD>" + webInputField("wifissid2", ssid2) + "</TD>"
  "</TR>"
  "<TR>"
    "<TD CLASS='LABEL'>Password</TD>"
    "<TD>" + webInputField("wifipass2", pass2, true) + "</TD>"
  "</TR>"
  "<TR><TH COLSPAN=2 CLASS='HEADING'>WiFi Network 3</TH></TR>"
  "<TR>"
    "<TD CLASS='LABEL'>SSID</TD>"
    "<TD>" + webInputField("wifissid3", ssid3) + "</TD>"
  "</TR>"
  "<TR>"
    "<TD CLASS='LABEL'>Password</TD>"
    "<TD>" + webInputField("wifipass3", pass3, true) + "</TD>"
  "</TR>"
  "<TR><TH COLSPAN=2 CLASS='HEADING'>This Web UI Login Credentials</TH></TR>"
  "<TR>"
    "<TD CLASS='LABEL'>Username</TD>"
    "<TD>" + webInputField("username", loginUsername) + "</TD>"
  "</TR>"
  "<TR>"
    "<TD CLASS='LABEL'>Password</TD>"
    "<TD>" + webInputField("password", loginPassword, true) + "</TD>"
  "</TR>"
  "<TR><TH COLSPAN=2 CLASS='HEADING'>Settings</TH></TR>"
  "<TR>"
    "<TD CLASS='LABEL'>Time Zone</TD>"
    "<TD>"
      "<SELECT NAME='utcoffset'>" + webUtcOffsetSelector() + "</SELECT>"
    "</TD>"
  "</TR>"
  "<TR>"
    "<TD CLASS='LABEL'>Clock Source</TD>"
    "<TD>"
      "<SELECT NAME='timesource'>" + webClockSourceSelector() + "</SELECT>"
    "</TD>"
  "</TR>"
  "<TR>"
    "<TD CLASS='LABEL'>Theme</TD>"
    "<TD>"
      "<SELECT NAME='theme'>" + webThemeSelector() + "</SELECT>"
    "</TD>"
  "</TR>"
  "<TR>"
    "<TD CLASS='LABEL'>Reverse Scrolling</TD>"
    "<TD><INPUT TYPE='CHECKBOX' NAME='scroll' VALUE='on'" +
    (scrollDirection<0? " CHECKED ":"") + "></TD>"
  "</TR>"
   "<TR>"
    "<TD CLASS='LABEL'>Zoomed Menu</TD>"
    "<TD><INPUT TYPE='CHECKBOX' NAME='zoom' VALUE='on'" +
    (zoomMenu? " CHECKED ":"") + "></TD>"
  "</TR>"
  "<TR><TH COLSPAN=2 CLASS='HEADING'>"
    "<INPUT TYPE='SUBMIT' VALUE='Save'>"
  "</TH></TR>"
  "</TABLE>"
"</FORM>"
"</DIV>"
"<DIV CLASS='table-card'>"
  "<H2>Backup & Restore</H2>"
  "<P><A HREF='/api/export/settings'>Download Settings Backup</A></P>"
  "<TEXTAREA STYLE='width:100%;min-height:150px;background:#0c141d;color:#edf6ff;border:1px solid #2a4a63;border-radius:12px;padding:12px;box-sizing:border-box'>" + webExportSettings() + "</TEXTAREA>"
  "<FORM ACTION='/api/import' METHOD='POST' STYLE='margin-top:14px'>"
    "<TEXTAREA NAME='backup' STYLE='width:100%;min-height:180px;background:#0c141d;color:#edf6ff;border:1px solid #2a4a63;border-radius:12px;padding:12px;box-sizing:border-box' PLACEHOLDER='volume=35&#10;theme=1&#10;utcOffset=16'></TEXTAREA>"
    "<DIV STYLE='margin-top:14px'><INPUT TYPE='SUBMIT' VALUE='Apply Backup'></DIV>"
  "</FORM>"
"</DIV>"
"</DIV>"
"</DIV>"
);
}
