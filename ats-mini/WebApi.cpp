#include "WebApi.h"
#include "Common.h"
#include "Utils.h"
#include "Storage.h"
#include "Themes.h"
#include "Menu.h"

#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoJson.h>


static const String jsonStatus()
{
  String ip = "";
  String ssid = "";
  if(WiFi.status() == WL_CONNECTED)
  {
    ip = WiFi.localIP().toString();
    ssid = WiFi.SSID();
  }
  else
  {
    ip = WiFi.softAPIP().toString();
    ssid = String(RECEIVER_NAME);
  }

  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();

  root["ip"] = ip;
  root["ssid"] = ssid;
  root["mac"] = getMACAddress();
  root["version"] = getVersion(true);
  root["band"] = getCurrentBand()->bandName;
  root["freq"] = freqToHz(currentFrequency, currentMode) + currentBFO;
  root["mode"] = bandModeDesc[currentMode];
  root["rssi"] = rssi;
  root["snr"] = snr;
  root["battery"] = batteryMonitor();

  String json;
  serializeJson(doc, json);
  return json;
}

static const String jsonMemory()
{
  JsonDocument doc;
  JsonArray memories_array = doc.to<JsonArray>();

  for(int i = 0; i < MEMORY_COUNT; i++)
  {
    JsonObject memObj = memories_array.add<JsonObject>();

    if(!memories[i].freq)
    {
      // Add empty object for unused memory slots
      continue;
    }

    memObj["id"] = i;
    memObj["freq"] = memories[i].freq;
    memObj["band"] = bands[memories[i].band].bandName;
    memObj["mode"] = bandModeDesc[memories[i].mode];
    memObj["name"] = memories[i].name;
  }

  String json;
  serializeJson(doc, json);
  return json;
}

const String jsonConfig()
{
  prefs.begin("network", true, STORAGE_PARTITION);
  String loginUsername = prefs.getString("loginusername", "");
  String loginPassword = prefs.getString("loginpassword", "");
  String ssid1 = prefs.getString("wifissid1", "");
  String pass1 = prefs.getString("wifipass1", "");
  String ssid2 = prefs.getString("wifissid2", "");
  String pass2 = prefs.getString("wifipass2", "");
  String ssid3 = prefs.getString("wifissid3", "");
  String pass3 = prefs.getString("wifipass3", "");
  prefs.end();

  JsonDocument doc;
  JsonObject config = doc.to<JsonObject>();

  config["username"] = loginUsername;
  config["password"] = loginPassword;
  config["wifissid1"] = ssid1;
  config["wifipass1"] = pass1;
  config["wifissid2"] = ssid2;
  config["wifipass2"] = pass2;
  config["wifissid3"] = ssid3;
  config["wifipass3"] = pass3;
  config["utcOffsetIdx"] = utcOffsetIdx;
  config["themeIdx"] = themeIdx;
  config["tuneHoldOff"] = tuneHoldOff;
  config["scrollDirection"] = scrollDirection;
  config["zoomMenu"] = zoomMenu;

  String json;
  serializeJson(doc, json);
  return json;
}

void jsonSetConfig(JsonDocument request)
{
  uint32_t prefsSave = 0;

  // Start modifying prefs
  prefs.begin("network", false, STORAGE_PARTITION);

  // Save user name and password
  if(request["username"].is<JsonVariant>() && request["password"].is<JsonVariant>())
  {
    String loginUsername = request["username"];
    String loginPassword = request["password"];

    prefs.putString("loginusername", loginUsername);
    prefs.putString("loginpassword", loginPassword);
  }

  // Save SSIDs and their passwords
  bool haveSSID = false;
  for(int j = 0; j < 3; j++)
  {
    char nameSSID[16], namePASS[16];

    sprintf(nameSSID, "wifissid%d", j + 1);
    sprintf(namePASS, "wifipass%d", j + 1);

    if(request[nameSSID].is<JsonVariant>() && request[namePASS].is<JsonVariant>())
    {
      String ssid = request[nameSSID];
      String pass = request[namePASS];
      prefs.putString(nameSSID, ssid);
      prefs.putString(namePASS, pass);
      haveSSID |= ssid != "" && pass != "";
    }
  }

  // Done with the prefs
  prefs.end();

  // Save time zone
  if(request["utcOffsetIdx"].is<int>())
  {
    utcOffsetIdx = request["utcOffsetIdx"];
    clockRefreshTime();
    prefsSave |= SAVE_SETTINGS;
  }

  // Save theme
  if(request["themeIdx"].is<int>())
  {
    themeIdx = request["themeIdx"];
    prefsSave |= SAVE_SETTINGS;
  }

  if(request["scrollDirection"].is<signed int>())
  {
    const unsigned int scrollDir = request["scrollDirection"].as<signed int>();
    if (scrollDir == -1 || scrollDir == 1)
    {
      scrollDirection = scrollDir;
      prefsSave |= SAVE_SETTINGS;
    }
  }

  if(request["tuneHoldOff"].is<int>())
  {
    tuneHoldOff = request["tuneHoldOff"];
    prefsSave |= SAVE_SETTINGS;
  }

  if(request["zoomMenu"].is<bool>())
  {
    zoomMenu = request["zoomMenu"];
    prefsSave |= SAVE_SETTINGS;
  }

  // Save preferences immediately
  prefsRequestSave(prefsSave, true);

  // If we are currently in AP mode, and infrastructure mode requested,
  // and there is at least one SSID / PASS pair, request network connection
  if(haveSSID && (wifiModeIdx > NET_AP_ONLY) && (WiFi.status() != WL_CONNECTED))
    netRequestConnect();
}

const String jsonConfigOptions()
{
  JsonDocument doc;

  JsonArray themes = doc["themes"].to<JsonArray>();
  for(int i = 0; i < getTotalThemes(); i++)
  {
    JsonObject themeObj = themes.add<JsonObject>();
    themeObj["id"] = i;
    themeObj["name"] = theme[i].name;
  }

  JsonArray offsets = doc["UTCOffsets"].to<JsonArray>();
  for(int i = 0; i < getTotalUTCOffsets(); i++)
  {
    JsonObject offsetObj = offsets.add<JsonObject>();
    offsetObj["id"] = i;
    offsetObj["offset"] = utcOffsets[i].offset;
    offsetObj["desc"] = utcOffsets[i].desc;
    offsetObj["city"] = utcOffsets[i].city;
  }

  String json;
  serializeJson(doc, json);
  return json;
}

bool checkApiAuth(AsyncWebServerRequest *request)
{
  prefs.begin("network", true, STORAGE_PARTITION);
  String loginUsername = prefs.getString("loginusername", "");
  String loginPassword = prefs.getString("loginpassword", "");
  prefs.end();

  if(loginUsername == "" && loginPassword == "") {
    return true;
  }

  return request->authenticate(loginUsername.c_str(), loginPassword.c_str());
}

void sendJsonResponse(AsyncWebServerRequest *request, int code, String content)
{
  AsyncWebServerResponse *response = request->beginResponse(code, "application/json", content);
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);
}

void addApiListeners(AsyncWebServer& server)
{
  server.on("/api/status", HTTP_GET, [] (AsyncWebServerRequest *request) {
    sendJsonResponse(request, 200, jsonStatus());
  });

  server.on("/api/memory", HTTP_GET, [] (AsyncWebServerRequest *request) {
    sendJsonResponse(request, 200, jsonMemory());
  });

  server.on("/api/config", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if(!checkApiAuth(request)) {
      return request->requestAuthentication();
    }
    sendJsonResponse(request, 200, jsonConfig());
  });

  server.on("/api/config", HTTP_POST,
    [] (AsyncWebServerRequest *request) {},
    NULL,
    [] (AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if(!checkApiAuth(request)) {
        return request->requestAuthentication();
      }

      JsonDocument jsonRequest;
      DeserializationError error = deserializeJson(jsonRequest, data, len);
      if (error)
      {
        sendJsonResponse(request, 400, "{\"error\":\"Invalid JSON\"}");
        return;
      }
      jsonSetConfig(jsonRequest);

      sendJsonResponse(request, 200, jsonConfig());
  });

  server.on("/api/configOptions", HTTP_GET, [] (AsyncWebServerRequest *request) {
    sendJsonResponse(request, 200, jsonConfigOptions());
  });

  server.on("/api", HTTP_OPTIONS, [] (AsyncWebServerRequest *request) {
    String allowedMethods = "GET, OPTIONS";
    if (request->url() == "/api/status" || request->url() == "/api/config")
    {
      allowedMethods += ", POST";
    }

    AsyncWebServerResponse *response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", allowedMethods);
    response->addHeader("Access-Control-Allow-Headers", request->header("Access-Control-Request-Headers"));
    request->send(response);
  });
}
