#include "Web.h"
#include "Common.h"
#include "Storage.h"
#include "Themes.h"
#include "Utils.h"
#include "Menu.h"
#include "Memories.h"
#include "Draw.h"
#include "Splash.h"
#include "Remote.h"
#include "BleMode.h"
#include "Ota.h"
#include "Patches.h"
#include "PageTemplate.h"
#include "PageCommon.h"
#include "PageStatus.h"
#include "PageMemory.h"
#include "PageConfig.h"
#include "PageWiFi.h"
#include "PageUpdate.h"
#include "PagePatches.h"

#include <new>
#include <mutex>
#include <atomic>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <time.h>
#include <esp_heap_caps.h>

#define SPLASH_MAX_FILE_SIZE (512U * 1024U)

static String loginUsername = "";
static String loginPassword = "";
static AsyncWebServer server(80);

static void webSetConfig(AsyncWebServerRequest *request);
static void webSetWiFi(AsyncWebServerRequest *request);
static void webSetMemories(AsyncWebServerRequest *request);
static void webUploadSplash(AsyncWebServerRequest *request, const String &filename,
                            size_t index, uint8_t *data, size_t len, bool final);
static bool webIsAuthenticated(AsyncWebServerRequest *request);
static void webUploadFirmwareComplete(AsyncWebServerRequest *request);
static void webUpdatePage(AsyncWebServerRequest *request, const OtaStatus &status = otaStatus(), int code = 0);
static void webUploadFirmware(AsyncWebServerRequest *request, const String &filename,
                              size_t index, uint8_t *data, size_t len, bool final);
static void webPatchesPage(AsyncWebServerRequest *request);
static void webPatchesComplete(AsyncWebServerRequest *request);
static void webUploadPatch(AsyncWebServerRequest *request, const String &filename,
                           size_t index, uint8_t *data, size_t len, bool final);
static bool webParseUTCDateTime(const String &text, uint32_t *epoch);

static String webPage(const char *body, const char *title, std::initializer_list<PageValue> values,
                      std::initializer_list<PageFragment> fragments = {});
static const String webRadioPage();
static const String webMemoryPage();
static const String webConfigPage();
static const String webWiFiPage();

// Apply Config settings on the main task, alongside receiver controls.
static struct
{
  std::mutex mutex;
  bool pending = false;
  int layout, region, stereo, brightness, sleepTimer, sleepMode, offTimer;
  int usbMode, tcpMode, bleMode, rds;
  int utcOffset, themeIndex;
  bool clamp, reverseScroll, halfStep, zoom;
  bool setClock;
  uint32_t epoch;
} configSettings;

static std::atomic<int> pendingWiFiMode{-1};

struct SplashUploadState
{
  bool incomplete;
  bool tooLarge;
};

static bool webIsAuthenticated(AsyncWebServerRequest *request)
{
  return(loginUsername == "" || loginPassword == "" ||
         request->authenticate(loginUsername.c_str(), loginPassword.c_str()));
}

// One download owns the PSRAM buffer until completion, disconnect, or failure.
static struct
{
  enum State { Pending, Ready, Failed } state = Pending;
  std::mutex mutex;
  uint8_t *bmp = nullptr;
  char filename[sizeof("screenshot-yyyy-mm-dd-hh-mm-ss.bmp")];

  // Caller holds mutex, including on capture timeout or failure.
  void release()
  {
    free(bmp);
    bmp = nullptr;
  }
} screenshot;

static const char *webScreenshotAcquire()
{
  std::unique_lock<std::mutex> lock(screenshot.mutex);
  if(screenshot.bmp) return "A screenshot download is already in progress.";
  screenshot.bmp = (uint8_t *)heap_caps_malloc(BMP_FILE_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if(!screenshot.bmp) return "Unable to allocate screenshot buffer.";
  screenshot.state = screenshot.Pending;

  // Yield to the main loop, but do not hold up the async web task indefinitely.
  uint32_t started = millis();
  while(screenshot.state == screenshot.Pending && millis() - started < 1000)
  {
    lock.unlock();
    delay(1);
    lock.lock();
  }
  if(screenshot.state == screenshot.Ready) return nullptr;
  screenshot.release();
  return screenshot.state == screenshot.Pending ? "Receiver is busy. Try again." : "Unable to capture screenshot.";
}

static void webScreenshotRelease()
{
  std::lock_guard<std::mutex> lock(screenshot.mutex);
  screenshot.release();
}

void webScreenshotTick()
{
  // Capture between redraws. A timeout cancels pending work; once ready,
  // the buffer stays unchanged until the download ends.
  std::lock_guard<std::mutex> lock(screenshot.mutex);
  if(screenshot.bmp && screenshot.state == screenshot.Pending)
  {
    strcpy(screenshot.filename, "screenshot.bmp");
    if(clockGetDate(nullptr, nullptr, nullptr, nullptr))
    {
      time_t now = time(NULL) + getCurrentUTCOffset() * 15 * 60;
      struct tm fields;
      gmtime_r(&now, &fields);
      strftime(screenshot.filename, sizeof(screenshot.filename), "screenshot-%Y-%m-%d-%H-%M-%S.bmp", &fields);
    }
    screenshot.state = remoteCaptureBmp(screenshot.bmp) ? screenshot.Ready : screenshot.Failed;
  }
}

// Apply queued web operations on the main task. Screenshot capture runs earlier
// in the loop so these potentially blocking operations cannot delay it.
bool webTick()
{
  otaTick();
  bool redraw = patchesTick();
  std::unique_lock<std::mutex> lock(configSettings.mutex);
  if(configSettings.pending)
  {
    utcOffsetIdx = configSettings.utcOffset;
    if(rdsModeIdx != configSettings.rds)
      doRDSMode(configSettings.rds - rdsModeIdx);
    if(configSettings.setClock) clockSetEpoch(configSettings.epoch);
    themeIdx = configSettings.themeIndex;
    scrollDirection = configSettings.reverseScroll? -1 : 1;
    zoomMenu = configSettings.zoom;
    setEncoderHalfStep(configSettings.halfStep);
    uiLayoutIdx = configSettings.layout;
    if(FmRegionIdx != configSettings.region)
      doFmRegion(configSettings.region - FmRegionIdx);
    if(fmStereoIdx != configSettings.stereo)
      doFmStereo(configSettings.stereo - fmStereoIdx);
    currentBrt = configSettings.brightness;
    doBrt(0);
    currentSleep = configSettings.sleepTimer;
    sleepModeIdx = configSettings.sleepMode;
    if(offTimerMinutes != configSettings.offTimer)
    {
      offTimerMinutes = configSettings.offTimer;
      offTimerRestart();
    }
    usbModeIdx = configSettings.usbMode;
    tcpModeIdx = configSettings.tcpMode;
    if(configSettings.bleMode == BLE_UNPAIR_ALL || bleModeIdx != configSettings.bleMode)
    {
      bleModeIdx = configSettings.bleMode;
      bleInit(bleModeIdx);
    }
    audioClamp = configSettings.clamp;
    digitalWrite(AUDIO_MUTE, audioClamp && (muteOn(MUTE_MAIN) || muteOn(MUTE_SQUELCH)));
    prefsRequestSave(SAVE_SETTINGS, true);
    configSettings.pending = false;
    redraw = true;
  }
  lock.unlock();

  int wifiMode = pendingWiFiMode.exchange(-1);
  if(wifiMode >= 0)
  {
    wifiModeIdx = wifiMode;
    prefsRequestSave(SAVE_SETTINGS, true);
    netInit(wifiModeIdx);
    redraw = true;
  }
  return redraw;
}

//
// Initialize internal web server
//
void webInit()
{
  // Reload authentication on each network initialization, but register routes once.
  prefs.begin("network", true, STORAGE_PARTITION);
  loginUsername = prefs.getString("loginusername", "");
  loginPassword = prefs.getString("loginpassword", "");
  prefs.end();

  static bool initialized = false;
  if(initialized) return;
  initialized = true;

  server.on("/", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/html", webRadioPage());
  });

  server.on("/memory", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if(!webIsAuthenticated(request)) return request->requestAuthentication();
    request->send(200, "text/html", webMemoryPage());
  });
  server.on("/memory", HTTP_POST, webSetMemories);

  server.on("/config", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    if(!webIsAuthenticated(request)) return request->requestAuthentication();
    request->send(200, "text/html", webConfigPage());
  });

  server.on("/wifi", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if(!webIsAuthenticated(request)) return request->requestAuthentication();
    request->send(200, "text/html", webWiFiPage());
  });
  server.on("/wifi", HTTP_POST, webSetWiFi);

  server.on("/screenshot.bmp", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if(!webIsAuthenticated(request)) return request->requestAuthentication();
    if(const char *error = webScreenshotAcquire())
      return request->send(503, "text/plain", error);
    AsyncWebServerResponse *response = request->beginResponse(200, "image/bmp", screenshot.bmp, BMP_FILE_SIZE);
    if(!response)
    {
      webScreenshotRelease();
      return request->send(503, "text/plain", "Unable to start screenshot download.");
    }
    // The server closes the connection after the response; aborts release it too.
    request->onDisconnect(webScreenshotRelease);
    response->addHeader("Content-Disposition", String("attachment; filename=\"") + screenshot.filename + "\"");
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });

  server.on("/splash.png", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if(!LittleFS.exists(SPLASH_PATH))
      return request->send(404, "text/plain", "Not found");
    request->send(LittleFS, SPLASH_PATH, "image/png");
  });

  server.onNotFound([] (AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
  });

  // This method saves configuration form contents
  server.on("/setconfig", HTTP_POST, webSetConfig, webUploadSplash);

  // Register subpaths first: the server also matches /update to /update/... .
  server.on("/update/upload", HTTP_POST, webUploadFirmwareComplete, webUploadFirmware);
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    if(!webIsAuthenticated(request)) return request->requestAuthentication();
    webUpdatePage(request, otaStatus(), 200);
  });
  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
    if(!webIsAuthenticated(request)) return request->requestAuthentication();
    if(!otaRequestLatest(request->hasParam("action", true) && request->getParam("action", true)->value() == "install"))
      return webUpdatePage(request, {OTA_FAILED, "An update is already in progress."}, 409);
    request->redirect("/update");
  });

  server.on("/patches", HTTP_GET, webPatchesPage);
  server.on("/patches", HTTP_POST, webPatchesComplete, webUploadPatch);

  // Start web server
  server.begin();
}

static void webUploadFirmware(AsyncWebServerRequest *request, const String &filename,
                              size_t index, uint8_t *data, size_t len, bool)
{
  if(!webIsAuthenticated(request) || request->getResponse()) return;

  if(!index)
  {
    if(!filename.endsWith(".bin"))
      return webUpdatePage(request, {OTA_FAILED, "Select a firmware .bin file."});
    const auto *param = request->getParam("size", true);
    if(!param) return webUpdatePage(request, {OTA_FAILED, "Invalid firmware size."});
    size_t imageSize = strtoul(param->value().c_str(), nullptr, 10);
    // Round-trip the number to reject signs, whitespace, suffixes, and overflow.
    if(!imageSize || imageSize > request->contentLength() || String(imageSize) != param->value())
      return webUpdatePage(request, {OTA_FAILED, "Invalid firmware size."});
    if(!otaBegin(imageSize))
      return webUpdatePage(request, {OTA_FAILED, "An update is already in progress."}, 409);
    request->client()->setRxTimeout(15);
    request->onDisconnect([]() { otaEndUpload(); });
  }

  if(!otaWrite(data, len)) return webUpdatePage(request);
}

static void webUploadFirmwareComplete(AsyncWebServerRequest *request)
{
  if(!webIsAuthenticated(request)) return request->requestAuthentication();
  if(request->getResponse()) return; // Preserve an upload error queued above.
  if(!request->hasParam("firmware", true, true))
    return webUpdatePage(request, {OTA_FAILED, "No complete firmware uploaded."});
  otaFinish();
  webUpdatePage(request);
}

static void webUploadSplash(AsyncWebServerRequest *request, const String &filename,
                            size_t index, uint8_t *data, size_t len, bool final)
{
  if(!webIsAuthenticated(request)) return;

  if(index == 0)
  {
    LittleFS.remove(SPLASH_TEMP_PATH);

    SplashUploadState *state = static_cast<SplashUploadState *>(calloc(1, sizeof(SplashUploadState)));
    if(!state) return;

    request->_tempObject = state;
    request->onDisconnect([request, state]() {
      if(state->incomplete)
        request->_tempFile.close();

      // Discard an upload that was not installed by webSetConfig().
      LittleFS.remove(SPLASH_TEMP_PATH);
    });

    // The browser filter is only advisory, so enforce the extension here too.
    if(filename.endsWith(".png"))
    {
      request->_tempFile = LittleFS.open(SPLASH_TEMP_PATH, "w");
      state->incomplete = request->_tempFile;
    }
  }

  SplashUploadState *state = static_cast<SplashUploadState *>(request->_tempObject);

  if(request->_tempFile && len)
  {
    if((index + len) > SPLASH_MAX_FILE_SIZE)
    {
      state->tooLarge = true;
      state->incomplete = false;
      request->_tempFile.close();
      LittleFS.remove(SPLASH_TEMP_PATH);
    }
    else if(request->_tempFile.write(data, len) != len)
    {
      state->incomplete = false;
      request->_tempFile.close();
      LittleFS.remove(SPLASH_TEMP_PATH);
    }
  }

  if(final && request->_tempFile)
    request->_tempFile.close();

  if(final && state)
    state->incomplete = false;
}

static bool webConfigNumber(AsyncWebServerRequest *request, const char *name, int &value,
                            int minimum, int maximum, int step = 1)
{
  if(!request->hasParam(name, true)) return false;
  const String &text = request->getParam(name, true)->value();
  if(!text.length()) return false;
  int number = 0;
  for(size_t i = 0; i < text.length(); i++)
  {
    if(text[i] < '0' || text[i] > '9') return false;
    number = number * 10 + text[i] - '0';
    if(number > maximum) return false;
  }
  if(number < minimum || (number - minimum) % step) return false;
  value = number;
  return true;
}

void webSetConfig(AsyncWebServerRequest *request)
{
  if(!webIsAuthenticated(request)) return request->requestAuthentication();

  int layout, region, stereo, brightness, sleepTimer, sleepMode, offTimer;
  int usbMode, tcpMode, bleMode, rds;
  int utcOffset, themeIndex;
  if(!webConfigNumber(request, "uilayout", layout, 0, getTotalUILayouts() - 1) ||
     !webConfigNumber(request, "fmregion", region, 0, getTotalFMRegions() - 1) ||
     !webConfigNumber(request, "fmstereo", stereo, 0, getTotalFMStereoModes() - 1) ||
     !webConfigNumber(request, "brightness", brightness, 10, 255, 5) ||
     !webConfigNumber(request, "sleeptimer", sleepTimer, 0, 255, 5) ||
     !webConfigNumber(request, "sleepmode", sleepMode, 0, getTotalSleepModes() - 1) ||
     !webConfigNumber(request, "offtimer", offTimer, 0, 240, 5) ||
     !webConfigNumber(request, "usbmode", usbMode, 0, getTotalUSBModes() - 1) ||
     !webConfigNumber(request, "tcpmode", tcpMode, 0, getTotalTCPModes() - 1) ||
     !webConfigNumber(request, "blemode", bleMode, 0, getTotalBluetoothModes() - 1) ||
     !webConfigNumber(request, "rds", rds, 0, getTotalRDSModes() - 1) ||
     !webConfigNumber(request, "utcoffset", utcOffset, 0, getTotalUTCOffsets() - 1) ||
     !webConfigNumber(request, "theme", themeIndex, 0, getTotalThemes() - 1))
    return request->send(400, "text/plain", "Invalid configuration value. Reload the Config page and try again.");

  uint32_t epoch;
  bool setClock = false;

  if(request->hasParam("datetime", true))
  {
    String dateTime = request->getParam("datetime", true)->value();
    if(dateTime != "")
    {
      if(!webParseUTCDateTime(dateTime, &epoch))
        return request->send(400, "text/plain", "Date/time must use the YYYY-mm-dd HH:MM:SS format and contain a valid UTC date and time.");
      setClock = true;
    }
  }

  if(request->hasParam("deletesplash", true))
  {
    LittleFS.remove(SPLASH_TEMP_PATH);
    LittleFS.remove(SPLASH_PATH);
  }
  else if(request->hasParam("splash", true, true))
  {
    String filename = request->getParam("splash", true, true)->value();

    if(filename != "")
    {
      SplashUploadState *state = static_cast<SplashUploadState *>(request->_tempObject);
      if(state && state->tooLarge)
        return request->send(413, "text/plain", "The splash image must not exceed 512 KB.");

      if(!filename.endsWith(".png"))
      {
        LittleFS.remove(SPLASH_TEMP_PATH);
        return request->send(400, "text/plain", "The splash image filename must end in .png.");
      }

      if(!LittleFS.exists(SPLASH_TEMP_PATH))
        return request->send(500, "text/plain", "The splash image could not be stored.");

      String error = splashValidate();
      if(error != "")
      {
        LittleFS.remove(SPLASH_TEMP_PATH);
        return request->send(400, "text/plain", error);
      }

      if(!LittleFS.rename(SPLASH_TEMP_PATH, SPLASH_PATH))
      {
        LittleFS.remove(SPLASH_TEMP_PATH);
        return request->send(500, "text/plain", "The splash image could not be installed.");
      }
    }
  }

  {
    std::lock_guard<std::mutex> lock(configSettings.mutex);
    configSettings.utcOffset = utcOffset;
    configSettings.themeIndex = themeIndex;
    configSettings.reverseScroll = request->hasParam("scroll", true);
    configSettings.zoom = request->hasParam("zoom", true);
    configSettings.halfStep = request->hasParam("encoderhalfstep", true);
    configSettings.rds = rds;
    configSettings.setClock = setClock;
    configSettings.epoch = setClock? epoch : 0;
    configSettings.layout = layout;
    configSettings.region = region;
    configSettings.stereo = stereo;
    configSettings.brightness = brightness;
    configSettings.sleepTimer = sleepTimer;
    configSettings.sleepMode = sleepMode;
    configSettings.offTimer = offTimer;
    configSettings.usbMode = usbMode;
    configSettings.tcpMode = tcpMode;
    configSettings.bleMode = bleMode;
    configSettings.clamp = request->hasParam("audioclamp", true);
    configSettings.pending = true;
  }

  // Show config page again
  request->redirect("/config");
}

static void webSetWiFi(AsyncWebServerRequest *request)
{
  if(!webIsAuthenticated(request)) return request->requestAuthentication();

  int wifiMode;
  if(!webConfigNumber(request, "wifimode", wifiMode, 0, getTotalWiFiModes() - 1))
    return request->send(400, "text/plain", "Invalid Wi-Fi mode.");

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

  // Save hidden SSID scanning preference
  bool wifiScanHidden = request->hasParam("wifiscanhidden", true);
  netSetScanHidden(wifiScanHidden);
  prefs.putBool("wifiscanhidden", wifiScanHidden);

  prefs.end();

  if(wifiMode != wifiModeIdx)
  {
    // Finish the response before switching off or reconfiguring its network.
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain",
      "Settings saved. Wi-Fi mode is changing. Reconnect to the receiver if needed. "
      "Off and Sync Only do not provide a web interface.");
    response->addHeader("Connection", "close");
    request->onDisconnect([wifiMode]() { pendingWiFiMode.store(wifiMode); });
    request->send(response);
    return;
  }

  request->redirect("/wifi");

  // If we are currently in AP mode, and infrastructure mode requested,
  // and there is at least one SSID / PASS pair, request network connection
  if(haveSSID && (wifiModeIdx>NET_AP_ONLY) && (WiFi.status()!=WL_CONNECTED))
    netRequestConnect();
}

static bool webParseUTCDateTime(const String &text, uint32_t *epoch)
{
  int year, month, day, hour, minute, second;
  return(epoch && text.length() == 19 &&
         sscanf(text.c_str(), "%4d-%2d-%2d %2d:%2d:%2d",
                &year, &month, &day, &hour, &minute, &second) == 6 &&
         clockUTCDateTimeToEpoch(year, month, day, hour, minute, second, epoch));
}

static String webPage(const char *body, const char *title, std::initializer_list<PageValue> values,
                      std::initializer_list<PageFragment> fragments)
{
  String page;
  pageReserve(page, sizeof(pageStart) + strlen(title) + strlen(body) + sizeof(pageEnd) + 64);
  pageAppend(page, pageStart, {{"title", title}});
  pageAppend(page, body, values, fragments);
  pageReserve(page, sizeof(pageEnd) + 64);
  // Snapshot after allocating the page, before the HTTP response copies it.
  multi_heap_info_t heap, psram;
  heap_caps_get_info(&heap, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  heap_caps_get_info(&psram, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  pageAppend(page, pageEnd, {
    {"heap_used", String(heap.total_allocated_bytes / 1024.0, 1)},
    {"heap_free", String(heap.total_free_bytes / 1024.0, 1)},
    {"psram_used", String(psram.total_allocated_bytes / 1024.0, 1)},
    {"psram_free", String(psram.total_free_bytes / 1024.0, 1)}
  });
  return page;
}

static const String webRadioPage()
{
  String ip = "";
  String ssid = "";
  String receiverTime = "Not synchronized";
  int offsetMinutes = getCurrentUTCOffset() * 15;
  int offsetMagnitude = abs(offsetMinutes);
  char utcOffset[10];
  snprintf(utcOffset, sizeof(utcOffset), "UTC%c%02d:%02d",
           offsetMinutes < 0? '-' : '+', offsetMagnitude / 60, offsetMagnitude % 60);
  String freq = currentMode == FM?
    String(currentFrequency / 1000000.0) + "MHz "
  : String(currentFrequency / 1000.0) + "kHz ";

  if(clockAvailable())
  {
    time_t localTime = time(NULL) + offsetMinutes * 60;
    struct tm fields;
    gmtime_r(&localTime, &fields);
    char text[20];

    strftime(text, sizeof(text),
             clockGetDate(NULL, NULL, NULL, NULL)? "%Y-%m-%d %H:%M:%S" : "%H:%M:%S",
             &fields);

    receiverTime = text;
  }

  receiverTime += " (" + String(utcOffset) + ")";

  if(WiFi.status()==WL_CONNECTED)
  {
    ip = WiFi.localIP().toString();
    ssid = WiFi.SSID();
  }
  else
  {
    ip = WiFi.softAPIP().toString();
    ssid = String(RECEIVER_NAME);
  }

  return webPage(pageStatus, pageStatusTitle, {
    {"title", pageStatusTitle},
    {"ip", ip}, {"ssid", ssid},
    {"mac", String(getMACAddress())}, {"version", String(getVersion(true))},
    {"time", receiverTime}, {"band", getCurrentBand()->bandName},
    {"frequency", freq}, {"mode", bandModeDesc[currentMode]},
    {"rssi", String(rssi)}, {"snr", String(snr)}, {"battery", String(batteryMonitor())}
  }, {
    {"navigation", [](String &out) { webNavigation(out, "/"); }}
  });
}

static void webSetMemories(AsyncWebServerRequest *request)
{
  if(!webIsAuthenticated(request)) return request->requestAuthentication();

  // Validate a complete replacement before modifying any live slots.
  Memory *pending = static_cast<Memory *>(ps_calloc(getTotalMemories(), sizeof(Memory)));
  if(!pending) return request->send(503, "text/plain", "Not enough memory to save the slots.");
  const char *error = nullptr;
  int slot = 0;
  for(; slot<getTotalMemories() ; slot++)
  {
    String suffix(slot);
    const auto *name = request->getParam("name" + suffix, true);
    const auto *band = request->getParam("band" + suffix, true);
    const auto *freq = request->getParam("freq" + suffix, true);
    const auto *mode = request->getParam("mode" + suffix, true);
    if(!name || !band || !freq || !mode)
    {
      error = "Missing slot fields.";
      break;
    }

    Memory &mem = pending[slot];
    const String &label = name->value();
    if(label.length() >= sizeof(mem.name)) error = "Names must be at most 9 characters.";
    for(size_t i=0 ; !error && i<label.length() ; i++)
      if((uint8_t)label[i]<0x20 || (uint8_t)label[i]>0x7e)
        error = "Names must contain printable ASCII characters.";
    if(error) break;
    memcpy(mem.name, label.c_str(), label.length());

    const String &number = freq->value();
    if(!number.length() || number.length()>10) error = "Invalid frequency in Hz.";
    uint64_t hz = 0;
    for(size_t i=0 ; !error && i<number.length() ; i++)
    {
      if(number[i]<'0' || number[i]>'9') error = "Invalid frequency in Hz.";
      else hz = hz * 10 + number[i] - '0';
    }
    if(hz>UINT32_MAX) error = "Frequency is out of range.";
    if(error) break;
    mem.freq = hz;

    mem.mode = AM;
    if(mode->value().length() || mem.freq)
    {
      mem.mode = 0xff;
      for(int i=0 ; i<getTotalModes() ; i++)
        if(mode->value() == bandModeDesc[i]) mem.mode = i;
      if(mem.mode == 0xff)
      {
        error = "Unknown mode.";
        break;
      }
    }

    mem.band = 0xff;
    for(int i=0 ; i<getTotalBands() ; i++)
    {
      if(band->value() != bands[i].bandName) continue;
      // Resolve duplicate band names using the frequency and mode.
      if(mem.freq && !isMemoryInBand(&bands[i], &mem)) continue;
      mem.band = i;
      break;
    }
    if(mem.band == 0xff)
    {
      if(!mem.freq && !band->value().length()) mem.band = 0;
      else
      {
        error = "Frequency or mode does not match the band.";
        break;
      }
    }
  }

  if(error)
  {
    free(pending);
    return request->send(400, "text/html", webPage(pageMemoryError, pageMemoryTitle, {
      {"title", pageMemoryTitle},
      {"slot", String(slot + 1)}, {"error", error}
    }));
  }

  setMemories(pending);
  free(pending);
  prefsRequestSave(SAVE_MEMORIES, true);
  request->redirect("/memory");
}

static const String webMemoryPage()
{
  return webPage(pageMemory, pageMemoryTitle, {
    {"title", pageMemoryTitle}, {"toolbar", pageMemoryToolbar}
  }, {
    {"navigation", [](String &out) { webNavigation(out, "/memory"); }},
    {"rows", [](String &out) {
      for(int j=0 ; j<MEMORY_COUNT ; j++)
      {
        const Memory mem = getMemory(j);
        const char *band = mem.freq && mem.band<getTotalBands()? bands[mem.band].bandName : "";
        char slot[4];
        snprintf(slot, sizeof(slot), "%02d", j+1);
        pageAppend(out, pageMemoryRow, {
          {"index", String(j)}, {"slot", slot}, {"name", mem.name}, {"frequency", String(mem.freq)},
          {"up_disabled", j==0? "DISABLED" : ""},
          {"down_disabled", j==getTotalMemories()-1? "DISABLED" : ""}
        }, {
          {"bands", [&](String &out) {
            for(int i=0 ; i<getTotalBands() ; i++)
            {
              bool duplicate = false;
              for(int k=0 ; k<i ; k++)
                if(!strcmp(bands[k].bandName, bands[i].bandName)) duplicate = true;
              if(duplicate) continue;
              pageAppend(out, pageMemoryOption, {
                {"value", bands[i].bandName}, {"selected", !strcmp(band, bands[i].bandName)? "SELECTED" : ""}
              });
            }
          }},
          {"modes", [&](String &out) {
            for(int i=0 ; i<getTotalModes() ; i++)
              pageAppend(out, pageMemoryOption, {
                {"value", bandModeDesc[i]}, {"selected", mem.freq && mem.mode==i? "SELECTED" : ""}
              });
          }}
        });
      }
    }}
  });
}

static void webConfigOptions(String &out, const char *const *labels, int count, int selected)
{
  for(int index = 0; index < count; index++)
  {
    pageAppend(out, pageConfigThemeOption, {
      {"value", String(index)}, {"selected", index==selected? "SELECTED" : ""}, {"label", labels[index]}
    });
  }
}

const String webConfigPage()
{
  int layout, region, stereo, brightness, sleepTimer, sleepMode, offTimer;
  int usbMode, tcpMode, bleMode, rds;
  int utcOffset, themeIndex;
  bool clamp, reverseScroll, halfStep, zoom;
  {
    // A redirect may arrive before the main task has applied the saved values.
    std::lock_guard<std::mutex> lock(configSettings.mutex);
    utcOffset = configSettings.pending ? configSettings.utcOffset : utcOffsetIdx;
    themeIndex = configSettings.pending ? configSettings.themeIndex : themeIdx;
    reverseScroll = configSettings.pending ? configSettings.reverseScroll : scrollDirection < 0;
    zoom = configSettings.pending ? configSettings.zoom : zoomMenu;
    halfStep = configSettings.pending ? configSettings.halfStep : encoderHalfStep;
    rds = configSettings.pending ? configSettings.rds : rdsModeIdx;
    layout = configSettings.pending ? configSettings.layout : uiLayoutIdx;
    region = configSettings.pending ? configSettings.region : FmRegionIdx;
    stereo = configSettings.pending ? configSettings.stereo : fmStereoIdx;
    brightness = configSettings.pending ? configSettings.brightness : currentBrt;
    sleepTimer = configSettings.pending ? configSettings.sleepTimer : currentSleep;
    sleepMode = configSettings.pending ? configSettings.sleepMode : sleepModeIdx;
    offTimer = configSettings.pending ? configSettings.offTimer : offTimerMinutes;
    usbMode = configSettings.pending ? configSettings.usbMode : usbModeIdx;
    tcpMode = configSettings.pending ? configSettings.tcpMode : tcpModeIdx;
    bleMode = configSettings.pending ? configSettings.bleMode : bleModeIdx;
    if(bleMode == BLE_UNPAIR_ALL) bleMode = BLE_OFF;
    clamp = configSettings.pending ? configSettings.clamp : audioClamp;
  }
  String splashResolution = String(spr.width()) + "x" + String(spr.height());

  return webPage(pageConfig, pageConfigTitle, {
    {"title", pageConfigTitle},
    {"brightness", String(brightness)}, {"sleep_timer", String(sleepTimer)},
    {"off_timer", String(offTimer)}, {"audio_clamp", clamp? "CHECKED" : ""},
    {"scroll", reverseScroll? "CHECKED" : ""},
    {"half_step", halfStep? "CHECKED" : ""}, {"zoom", zoom? "CHECKED" : ""},
    {"resolution", splashResolution}
  }, {
    {"navigation", [](String &out) { webNavigation(out, "/config"); }},
    {"usb_options", [&](String &out) { webConfigOptions(out, usbModeDesc, getTotalUSBModes(), usbMode); }},
    {"tcp_options", [&](String &out) { webConfigOptions(out, tcpModeDesc, getTotalTCPModes(), tcpMode); }},
    {"ble_options", [&](String &out) { webConfigOptions(out, bleModeDesc, getTotalBluetoothModes(), bleMode); }},
    {"layout_options", [&](String &out) { webConfigOptions(out, uiLayoutDesc, getTotalUILayouts(), layout); }},
    {"rds_options", [&](String &out) {
      for(int i=0 ; i<getTotalRDSModes(); i++)
        pageAppend(out, pageConfigThemeOption, {
          {"value", String(i)}, {"selected", rds==i? "SELECTED" : ""}, {"label", rdsMode[i].desc}
        });
    }},
    {"region_options", [&](String &out) {
      for(int i=0 ; i<getTotalFMRegions(); i++)
        pageAppend(out, pageConfigThemeOption, {
          {"value", String(i)}, {"selected", region==i? "SELECTED" : ""}, {"label", fmRegions[i].desc}
        });
    }},
    {"stereo_options", [&](String &out) { webConfigOptions(out, fmStereoDesc, getTotalFMStereoModes(), stereo); }},
    {"sleep_options", [&](String &out) { webConfigOptions(out, sleepModeDesc, getTotalSleepModes(), sleepMode); }},
    {"utc_options", [&](String &out) {
      for(int i=0 ; i<getTotalUTCOffsets(); i++)
        pageAppend(out, pageConfigUtcOption, {
          {"value", String(i)}, {"minutes", String(utcOffsets[i].offset * 15)},
          {"selected", utcOffset==i? "SELECTED" : ""}, {"label", utcOffsets[i].desc}
        });
    }},
    {"theme_options", [&](String &out) {
      for(int i=0 ; i<getTotalThemes(); i++)
        pageAppend(out, pageConfigThemeOption, {
          {"value", String(i)}, {"selected", themeIndex==i? "SELECTED" : ""}, {"label", theme[i].name}
        });
    }},
    {"splash", [](String &out) {
      if(LittleFS.exists(SPLASH_PATH)) pageAppend(out, pageSplash, {{"version", String(millis())}});
      else out += "Not installed";
    }}
  });
}

static const String webWiFiPage()
{
  prefs.begin("network", true, STORAGE_PARTITION);
  String ssid1 = prefs.getString("wifissid1", "");
  String pass1 = prefs.getString("wifipass1", "");
  String ssid2 = prefs.getString("wifissid2", "");
  String pass2 = prefs.getString("wifipass2", "");
  String ssid3 = prefs.getString("wifissid3", "");
  String pass3 = prefs.getString("wifipass3", "");
  bool scanHidden = prefs.getBool("wifiscanhidden", false);
  prefs.end();

  return webPage(pageWiFi, pageWiFiTitle, {
    {"title", pageWiFiTitle},
    {"username", loginUsername}, {"password", loginPassword},
    {"ssid1", ssid1}, {"pass1", pass1}, {"ssid2", ssid2}, {"pass2", pass2},
    {"ssid3", ssid3}, {"pass3", pass3}, {"scan_hidden", scanHidden? "CHECKED" : ""}
  }, {
    {"navigation", [](String &out) { webNavigation(out, "/wifi"); }},
    {"wifi_options", [](String &out) {
      webConfigOptions(out, wifiModeDesc, getTotalWiFiModes(), wifiModeIdx);
    }}
  });
}

// Explicit request errors leave the active operation's status unchanged.
static void webUpdatePage(AsyncWebServerRequest *request, const OtaStatus &status, int code)
{
  const bool busy = status.phase == OTA_CHECK_QUEUED || status.phase == OTA_QUEUED ||
                    status.phase == OTA_CONNECTING || status.phase == OTA_WRITING;
  const bool complete = status.phase == OTA_COMPLETE || status.phase == OTA_REBOOT_PENDING;
  const bool available = status.phase == OTA_AVAILABLE;
  const char *refresh = complete? pageUpdateComplete : busy? pageUpdateBusy : "";
  const String page = webPage(pageUpdate, pageUpdateTitle, {
    {"title", pageUpdateTitle},
    {"message", status.message},
    {"action", available? "install" : "check"}, {"disabled", busy || complete? "DISABLED" : ""},
    {"button", available? "Update" : "Check for updates"}, {"refresh", refresh}
  }, {
    {"navigation", [](String &out) { webNavigation(out, "/update"); }}
  });
  if(!code) code = status.phase == OTA_FAILED? 400 : 200;
  AsyncWebServerResponse *response = request->beginResponse(code, "text/html", page);
  response->addHeader("Cache-Control", "no-store");
  response->addHeader("Connection", "close");
  request->send(response);
}

static PatchUpload *webPatchBegin(AsyncWebServerRequest *request)
{
  if(request->_tempObject) return static_cast<PatchUpload *>(request->_tempObject);
  PatchUpload *state = new(std::nothrow) PatchUpload();
  if(!state) return nullptr;
  request->_tempObject = state;
  request->onDisconnect([request, state]() {
    // Destroy the C++ upload context before the server frees _tempObject.
    request->_tempObject = nullptr;
    delete state;
  });
  if(!request->hasParam("slot", true) || !request->hasParam("action", true))
  {
    state->error = "Missing patch slot or action.";
    return state;
  }
  String slot = request->getParam("slot", true)->value();
  String action = request->getParam("action", true)->value();
  if(slot.length() != 1 || slot[0] < '0' || slot[0] > '3' ||
     action != "upload")
  {
    state->error = "Invalid patch slot or action.";
    return state;
  }
  uint8_t mode = PATCH_MODE_COUNT;
  if(request->hasParam("mode", true))
    for(uint8_t i = 0; i < PATCH_MODE_COUNT; i++)
      if(request->getParam("mode", true)->value() == patchModeNames[i]) mode = i;
  state->begin(slot[0] - '0', static_cast<PatchMode>(mode));
  return state;
}

static void webUploadPatch(AsyncWebServerRequest *request, const String &filename,
                           size_t index, uint8_t *data, size_t len, bool final)
{
  if(!webIsAuthenticated(request) || request->getResponse()) return;
  if(filename.isEmpty() && index == 0 && len == 0 && final) return;
  PatchUpload *state = webPatchBegin(request);
  if(!state) return request->send(503, "text/plain", "Not enough memory to process the patch set.");
  if(state->error) return;
  if(index == 0)
  {
    String extension = filename;
    extension.toLowerCase();
    if(!extension.endsWith(".bin"))
    {
      state->error = "Upload exactly one .bin patch file.";
      return;
    }
  }

  state->write(index, data, len, final);
}

static void webPatchesComplete(AsyncWebServerRequest *request)
{
  if(!webIsAuthenticated(request)) return request->requestAuthentication();
  if(request->getResponse()) return;
  if(request->hasParam("delete", true))
  {
    PatchUpload *state = static_cast<PatchUpload *>(request->_tempObject);
    if(state)
    {
      // Discard any uploaded file and release its reservation before deletion.
      state->error = "Upload discarded.";
      state->finish();
    }
    const char *error = nullptr;
    String slot = request->hasParam("slot", true)? request->getParam("slot", true)->value() : "";
    if(slot.length() != 1 || slot[0] < '1' || slot[0] > '3') error = "Invalid patch slot.";
    else patchesDelete(slot[0] - '0', error);
    if(error) return request->send(400, "text/plain", error);
    return request->redirect("/patches");
  }
  if(!request->_tempObject)
  {
    if(!request->hasParam("slot", true) || !request->hasParam("action", true))
      return request->send(400, "text/plain", "Missing patch slot or action.");
    String slot = request->getParam("slot", true)->value();
    String action = request->getParam("action", true)->value();
    const char *error = nullptr;
    if(slot.length() != 1 || slot[0] < '0' || slot[0] > '3') error = "Invalid patch slot.";
    else if(action == "select") patchesRequestSelection(slot[0] - '0', error);
    else error = "Upload one complete .bin patch.";
    if(error) return request->send(400, "text/plain", error);
    return request->redirect("/patches");
  }
  PatchUpload *state = static_cast<PatchUpload *>(request->_tempObject);
  state->finish();
  if(state->error)
    return request->send(400, "text/plain", state->error);
  request->redirect("/patches");
}

static void webPatchesPage(AsyncWebServerRequest *request)
{
  if(!webIsAuthenticated(request)) return request->requestAuthentication();
  bool busy;
  uint8_t selected;
  patchesSnapshot(selected, busy);
  const String page = webPage(pagePatches, pagePatchesTitle, {
    {"title", pagePatchesTitle}, {"busy", busy? pagePatchesBusy : ""},
    {"disabled", busy? "DISABLED" : ""}
  }, {
    {"navigation", [](String &out) { webNavigation(out, "/patches"); }},
    {"options", [&](String &out) {
      for(uint8_t slot = 0; slot <= PATCH_SET_COUNT; slot++)
      {
        if(slot && !patchesModes(slot)) continue;
        pageAppend(out, pagePatchOption, {
          {"value", String(slot)}, {"selected", slot == selected? "SELECTED" : ""}, {"label", patchSetNames[slot]}
        });
      }
    }},
    {"sets", [&](String &out) {
      for(uint8_t slot = 1; slot <= PATCH_SET_COUNT; slot++)
      {
        const char *disabled = busy || selected == slot? "DISABLED" : "";
        uint8_t modes = patchesModes(slot);
        String installed;
        for(uint8_t mode = 0; mode < PATCH_MODE_COUNT; mode++)
        {
          if(mode) installed += " · ";
          installed += patchModeNames[mode];
          installed += (modes & (1U << mode))? ": uploaded" : ": Default";
        }
        pageAppend(out, pagePatchSet, {
          {"slot", String(slot)}, {"name", patchSetNames[slot]}, {"installed", installed}, {"disabled", disabled}
        }, {
          {"options", [](String &out) {
            for(uint8_t mode = 0; mode < PATCH_MODE_COUNT; mode++)
              pageAppend(out, pagePatchOption, {
                {"value", patchModeNames[mode]}, {"selected", ""}, {"label", patchModeNames[mode]}
              });
          }},
          {"delete", [&](String &out) {
            if(modes) pageAppend(out, pagePatchDelete, {{"slot", String(slot)}, {"disabled", disabled}});
          }}
        });
      }
    }}
  });
  AsyncWebServerResponse *response = request->beginResponse(200, "text/html", page);
  response->addHeader("Cache-Control", "no-store");
  if(busy) response->addHeader("Refresh", "2; url=/patches");
  request->send(response);
}
