#include "JsonAPI.h"
#include "Remote.h"
#include "Menu.h"
#include "Utils.h"
#include <ArduinoJson.h>

#define JSON_BUFFER_SIZE 512

static char jsonBuffer[JSON_BUFFER_SIZE];
static uint16_t jsonBufferPos = 0;

static bool jsonApiReadWrite = false;
static uint32_t jsonReadWriteUntil = 0;

static bool jsonBusy = false;
static const char* jsonBusyOperation = nullptr;

static void jsonCheckReadWriteTimeout()
{
  if(jsonApiReadWrite && millis() > jsonReadWriteUntil)
  {
    jsonApiReadWrite = false;
    jsonReadWriteUntil = 0;
  }
}

static bool jsonIsReadWrite()
{
  jsonCheckReadWriteTimeout();
  return jsonApiReadWrite;
}

static void jsonRefreshReadWriteTimeout()
{
  jsonApiReadWrite = true;
  jsonReadWriteUntil = millis() + JSON_API_RW_TIMEOUT_MS;
}

static uint32_t jsonReadWriteRemainingMs()
{
  jsonCheckReadWriteTimeout();
  if(!jsonApiReadWrite) return 0;
  uint32_t now = millis();
  if(now >= jsonReadWriteUntil) return 0;
  return jsonReadWriteUntil - now;
}

static void jsonSendResponse(Stream* stream, JsonDocument& doc)
{
  serializeJson(doc, *stream);
  stream->println();
}

static void jsonSendError(Stream* stream, const char* error, const char* command = nullptr)
{
  StaticJsonDocument<256> doc;
  doc["status"] = "error";
  doc["protocol"] = JSON_API_VERSION;
  doc["readonly"] = !jsonIsReadWrite();
  doc["busy"] = jsonBusy;
  if(jsonBusyOperation) doc["operation"] = jsonBusyOperation;
  doc["error"] = error;
  if(command) doc["command"] = command;

  jsonSendResponse(stream, doc);
}

static void jsonSendEvent(Stream* stream, const char* eventName, const char* operation)
{
  StaticJsonDocument<256> doc;
  doc["status"] = "event";
  doc["protocol"] = JSON_API_VERSION;
  doc["event"] = eventName;
  doc["busy"] = jsonBusy;
  if(operation) doc["operation"] = operation;
  doc["readonly"] = !jsonIsReadWrite();

  jsonSendResponse(stream, doc);
}

static void jsonSendStatus(Stream* stream, bool full = false)
{
  StaticJsonDocument<JSON_BUFFER_SIZE> doc;

  jsonCheckReadWriteTimeout();

  rx.getCurrentReceivedSignalQuality();
  uint8_t currentRssi = rx.getCurrentRSSI();
  uint8_t currentSnr = rx.getCurrentSNR();

  int32_t effectiveFrequencyHz = ((int32_t)currentFrequency * 1000L);
  if(isSSB()) effectiveFrequencyHz += currentBFO;

  doc["status"] = "ok";
  doc["protocol"] = JSON_API_VERSION;
  doc["readonly"] = !jsonApiReadWrite;
  doc["rw_remaining_ms"] = jsonReadWriteRemainingMs();
  doc["busy"] = jsonBusy;
  if(jsonBusyOperation) doc["operation"] = jsonBusyOperation;

  doc["frequency_hz"] = effectiveFrequencyHz;
  doc["frequency_khz"] = currentFrequency;
  doc["mode"] = bandModeDesc[currentMode];
  doc["band"] = getCurrentBand()->bandName;
  doc["volume"] = volume;
  doc["rssi"] = currentRssi;
  doc["snr"] = currentSnr;
  doc["bfo"] = isSSB() ? currentBFO : 0;
  doc["bandwidth"] = getCurrentBandwidth()->desc;
  doc["step"] = getCurrentStep()->desc;
  doc["agc"] = agcIdx;
  doc["squelch"] = currentSquelch;

  if(full)
  {
    rx.getFrequency();
    doc["tuning_cap"] = rx.getAntennaTuningCapacitor();
    doc["battery"] = batteryMonitor();

    Band* band = getCurrentBand();
    doc["band_min_khz"] = band->minimumFreq;
    doc["band_max_khz"] = band->maximumFreq;
  }

  jsonSendResponse(stream, doc);
}

static bool jsonRequireReadWrite(Stream* stream, const char* command)
{
  if(!jsonIsReadWrite())
  {
    jsonSendError(stream, "API in read-only mode, use enable_readwrite first", command);
    return false;
  }

  jsonRefreshReadWriteTimeout();
  return true;
}

static int cmdPing(Stream* stream, JsonDocument& req)
{
  StaticJsonDocument<256> doc;
  doc["status"] = "ok";
  doc["protocol"] = JSON_API_VERSION;
  doc["pong"] = true;
  doc["readonly"] = !jsonIsReadWrite();
  doc["rw_remaining_ms"] = jsonReadWriteRemainingMs();
  doc["busy"] = jsonBusy;
  if(jsonBusyOperation) doc["operation"] = jsonBusyOperation;

  jsonSendResponse(stream, doc);
  return REMOTE_CHANGED;
}

static int cmdHelp(Stream* stream, JsonDocument& req)
{
  StaticJsonDocument<JSON_BUFFER_SIZE> doc;

  bool rw = jsonIsReadWrite();

  doc["status"] = "ok";
  doc["protocol"] = JSON_API_VERSION;
  doc["readonly"] = !rw;
  doc["rw_timeout_ms"] = JSON_API_RW_TIMEOUT_MS;
  doc["rw_remaining_ms"] = jsonReadWriteRemainingMs();
  doc["busy"] = jsonBusy;
  if(jsonBusyOperation) doc["operation"] = jsonBusyOperation;

  JsonArray cmds = doc.createNestedArray("commands");
  cmds.add("help");
  cmds.add("ping");
  cmds.add("get_status");
  cmds.add("get_status_full");
  cmds.add("enable_readwrite");
  cmds.add("disable_readwrite");

  if(rw)
  {
    cmds.add("set_frequency");
    cmds.add("set_mode");
    cmds.add("set_volume");
    cmds.add("set_bfo");
    cmds.add("seek_up");
    cmds.add("seek_down");
  }

  JsonArray modes = doc.createNestedArray("modes");
  modes.add("FM");
  modes.add("AM");
  modes.add("LSB");
  modes.add("USB");

  doc["frequency_unit"] = "Hz";
  doc["volume_range"] = "0-63";
  doc["bfo_unit"] = "Hz";
  doc["note"] = "For SSB prefer explicit bfo parameter to avoid sub-kHz ambiguity";

  jsonSendResponse(stream, doc);
  return REMOTE_CHANGED;
}

static int cmdEnableReadWrite(Stream* stream, JsonDocument& req)
{
#if JSON_API_REQUIRE_TOKEN
  if(!req["token"].is<const char*>())
  {
    jsonSendError(stream, "Missing token", "enable_readwrite");
    return 0;
  }

  const char* token = req["token"];
  if(strcmp(token, JSON_API_TOKEN) != 0)
  {
    jsonSendError(stream, "Invalid token", "enable_readwrite");
    return 0;
  }
#endif

  jsonRefreshReadWriteTimeout();

  StaticJsonDocument<256> doc;
  doc["status"] = "ok";
  doc["protocol"] = JSON_API_VERSION;
  doc["readonly"] = false;
  doc["rw_timeout_ms"] = JSON_API_RW_TIMEOUT_MS;
  doc["rw_remaining_ms"] = jsonReadWriteRemainingMs();
  doc["message"] = "Read-write mode enabled";

  jsonSendResponse(stream, doc);
  return REMOTE_CHANGED;
}

static int cmdDisableReadWrite(Stream* stream, JsonDocument& req)
{
  jsonApiReadWrite = false;
  jsonReadWriteUntil = 0;

  StaticJsonDocument<256> doc;
  doc["status"] = "ok";
  doc["protocol"] = JSON_API_VERSION;
  doc["readonly"] = true;
  doc["message"] = "Read-write mode disabled";

  jsonSendResponse(stream, doc);
  return REMOTE_CHANGED;
}

static int cmdGetStatus(Stream* stream, JsonDocument& req)
{
  bool full = req["full"] | false;
  jsonSendStatus(stream, full);
  return REMOTE_CHANGED;
}

static int cmdGetStatusFull(Stream* stream, JsonDocument& req)
{
  jsonSendStatus(stream, true);
  return REMOTE_CHANGED;
}

static int cmdSetFrequency(Stream* stream, JsonDocument& req)
{
  if(!jsonRequireReadWrite(stream, "set_frequency")) return 0;

  if(!req["frequency"].is<uint32_t>())
  {
    jsonSendError(stream, "Missing frequency parameter in Hz", "set_frequency");
    return 0;
  }

  uint32_t freqHz = req["frequency"].as<uint32_t>();
  if(freqHz == 0)
  {
    jsonSendError(stream, "Invalid frequency", "set_frequency");
    return 0;
  }

  Band* band = getCurrentBand();
  uint16_t targetFreq = (uint16_t)(freqHz / 1000UL);
  int targetBfo = 0;

  if(isSSB())
  {
    if(req["bfo"].is<int>())
    {
      targetBfo = req["bfo"].as<int>();
    }
    else
    {
      targetBfo = (int)(freqHz % 1000UL);
      if(targetBfo > 500) targetBfo -= 1000;
    }

    if(abs(targetBfo) > MAX_BFO)
    {
      jsonSendError(stream, "BFO out of range", "set_frequency");
      return 0;
    }
  }

  if(!isFreqInBand(band, targetFreq) || (isSSB() && targetFreq == band->maximumFreq && targetBfo))
  {
    jsonSendError(stream, "Frequency out of band range", "set_frequency");
    return 0;
  }

  if(!updateFrequency(targetFreq, false))
  {
    jsonSendError(stream, "Failed to set frequency", "set_frequency");
    return 0;
  }

  if(isSSB())
    updateBFO(targetBfo, false);
  else if(currentBFO)
    updateBFO(0, true);

  clearStationInfo();
  identifyFrequency(currentFrequency + currentBFO / 1000);

  jsonSendStatus(stream, false);
  return REMOTE_CHANGED | REMOTE_PREFS;
}

static int cmdSetMode(Stream* stream, JsonDocument& req)
{
  if(!jsonRequireReadWrite(stream, "set_mode")) return 0;

  if(!req["mode"].is<const char*>())
  {
    jsonSendError(stream, "Missing mode parameter", "set_mode");
    return 0;
  }

  const char* modeStr = req["mode"];
  uint8_t newMode = 0xFF;

  for(uint8_t i = 0; i < getTotalModes(); i++)
  {
    if(strcasecmp(bandModeDesc[i], modeStr) == 0)
    {
      newMode = i;
      break;
    }
  }

  if(newMode == 0xFF)
  {
    jsonSendError(stream, "Invalid mode, use AM/LSB/USB", "set_mode");
    return 0;
  }

  if(currentMode == FM || newMode == FM)
  {
    jsonSendError(stream, "Cannot change from/to FM mode via API", "set_mode");
    return 0;
  }

  bands[bandIdx].currentFreq = currentFrequency + currentBFO / 1000;
  bands[bandIdx].currentStepIdx = defaultStepIdx[newMode];
  bands[bandIdx].bandwidthIdx = defaultBwIdx[newMode];
  bands[bandIdx].bandMode = newMode;

  selectBand(bandIdx);

  jsonSendStatus(stream, false);
  return REMOTE_CHANGED | REMOTE_PREFS;
}

static int cmdSetVolume(Stream* stream, JsonDocument& req)
{
  if(!jsonRequireReadWrite(stream, "set_volume")) return 0;

  if(!req["volume"].is<int>())
  {
    jsonSendError(stream, "Missing volume parameter", "set_volume");
    return 0;
  }

  int vol = req["volume"].as<int>();
  if(vol < 0 || vol > 63)
  {
    jsonSendError(stream, "Volume must be 0-63", "set_volume");
    return 0;
  }

  volume = vol;
  if(!muteOn(MUTE_MAIN)) rx.setVolume(volume);

  jsonSendStatus(stream, false);
  return REMOTE_CHANGED | REMOTE_PREFS;
}

static int cmdSetBfo(Stream* stream, JsonDocument& req)
{
  if(!jsonRequireReadWrite(stream, "set_bfo")) return 0;

  if(!isSSB())
  {
    jsonSendError(stream, "BFO only available in SSB mode", "set_bfo");
    return 0;
  }

  if(!req["bfo"].is<int>())
  {
    jsonSendError(stream, "Missing bfo parameter", "set_bfo");
    return 0;
  }

  int bfo = req["bfo"].as<int>();
  if(abs(bfo) > MAX_BFO)
  {
    jsonSendError(stream, "BFO out of range", "set_bfo");
    return 0;
  }

  updateBFO(bfo, false);

  jsonSendStatus(stream, false);
  return REMOTE_CHANGED | REMOTE_PREFS;
}

static int jsonDoSeek(Stream* stream, bool up, const char* command)
{
  if(!jsonRequireReadWrite(stream, command)) return 0;

  if(isSSB())
  {
    jsonSendError(stream, "Seek not available in SSB mode", command);
    return 0;
  }

  jsonBusy = true;
  jsonBusyOperation = command;
  jsonSendEvent(stream, up ? "seek_up_started" : "seek_down_started", command);
  stream->flush();

  clearStationInfo();
  rssi = snr = 0;

  muteOn(MUTE_TEMP, true);
  consumeAbortPending();
  rx.seekStationProgress(showFrequencySeek, consumeAbortPending, up ? 1 : 0);
  updateFrequency(rx.getFrequency(), true);

  clearStationInfo();
  identifyFrequency(currentFrequency);
  muteOn(MUTE_TEMP, false);

  jsonBusy = false;
  jsonBusyOperation = nullptr;

  jsonSendStatus(stream, false);
  return REMOTE_CHANGED | REMOTE_PREFS;
}

static int cmdSeekUp(Stream* stream, JsonDocument& req)
{
  return jsonDoSeek(stream, true, "seek_up");
}

static int cmdSeekDown(Stream* stream, JsonDocument& req)
{
  return jsonDoSeek(stream, false, "seek_down");
}

int jsonProcessCommand(Stream* stream)
{
  jsonCheckReadWriteTimeout();

  while(stream->available())
  {
    char c = stream->read();

    if(c == '\n' || c == '\r')
    {
      if(jsonBufferPos == 0) continue;

      jsonBuffer[jsonBufferPos] = '\0';

      StaticJsonDocument<JSON_BUFFER_SIZE> doc;
      DeserializationError error = deserializeJson(doc, jsonBuffer);

      jsonBufferPos = 0;

      if(error)
      {
        jsonSendError(stream, error.c_str());
        return 0;
      }

      if(!doc["cmd"].is<const char*>())
      {
        jsonSendError(stream, "Missing cmd field");
        return 0;
      }

      const char* cmd = doc["cmd"];

      if(strcmp(cmd, "help") == 0)
        return cmdHelp(stream, doc);
      else if(strcmp(cmd, "ping") == 0)
        return cmdPing(stream, doc);
      else if(strcmp(cmd, "get_status") == 0)
        return cmdGetStatus(stream, doc);
      else if(strcmp(cmd, "get_status_full") == 0)
        return cmdGetStatusFull(stream, doc);
      else if(strcmp(cmd, "enable_readwrite") == 0)
        return cmdEnableReadWrite(stream, doc);
      else if(strcmp(cmd, "disable_readwrite") == 0)
        return cmdDisableReadWrite(stream, doc);
      else if(strcmp(cmd, "set_frequency") == 0)
        return cmdSetFrequency(stream, doc);
      else if(strcmp(cmd, "set_mode") == 0)
        return cmdSetMode(stream, doc);
      else if(strcmp(cmd, "set_volume") == 0)
        return cmdSetVolume(stream, doc);
      else if(strcmp(cmd, "set_bfo") == 0)
        return cmdSetBfo(stream, doc);
      else if(strcmp(cmd, "seek_up") == 0)
        return cmdSeekUp(stream, doc);
      else if(strcmp(cmd, "seek_down") == 0)
        return cmdSeekDown(stream, doc);

      jsonSendError(stream, "Unknown command", cmd);
      return 0;
    }
    else if(jsonBufferPos >= JSON_BUFFER_SIZE - 1)
    {
      jsonSendError(stream, "Command too long");
      jsonBufferPos = 0;
      return 0;
    }
    else
    {
      jsonBuffer[jsonBufferPos++] = c;
    }
  }

  return 0;
}
