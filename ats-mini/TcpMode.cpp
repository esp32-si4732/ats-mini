#include "Common.h"
#include "Remote.h"
#include "TcpMode.h"

#include <WiFi.h>

static WiFiServer tcpServer(TCP_CONTROL_PORT, 1);
static WiFiClient tcpClient;
static RemoteState remoteTCPState;
static uint32_t lastListenAttempt;
static bool tcpInputResetPending = true;

static bool tcpIsConnected()
{
  if(!tcpClient.connected())
  {
    tcpClient.stop();
    return false;
  }
  return true;
}

static InputEvent tcpReadInput(uint8_t& byte)
{
  if(tcpInputResetPending)
  {
    tcpInputResetPending = false;
    return InputEvent::SessionReset;
  }
  if(tcpClient.fd() < 0) return InputEvent::NoData;

  // Drain received bytes before checking EOF, without probing the socket for
  // every byte of a command.
  int value = tcpClient.read();
  if(value < 0)
    return tcpIsConnected() ? InputEvent::NoData : InputEvent::SessionReset;
  byte = uint8_t(value);
  return InputEvent::Byte;
}

void tcpStop()
{
  tcpClient.stop();
  tcpServer.end();
  tcpInputResetPending = true;
  remoteTCPState = RemoteState{};
}

static bool tcpSelectMode(uint8_t tcpMode)
{
  static uint8_t previousMode = TCP_OFF;
  if(tcpMode != previousMode)
  {
    // Reconnect after switching protocols; queued input belongs to the old one.
    tcpStop();
    previousMode = tcpMode;
  }
  return tcpMode == TCP_ADHOC || tcpMode == TCP_KENWOOD;
}

bool tcpLoop(uint8_t tcpMode)
{
  bool networkUp = WiFi.status() == WL_CONNECTED || (WiFi.getMode() & WIFI_AP);
  if(!tcpSelectMode(tcpMode) || !networkUp)
  {
    if(tcpServer) tcpStop();
    return false;
  }

  if(!tcpServer)
  {
    if(millis() - lastListenAttempt < 1000) return false;
    lastListenAttempt = millis();
    tcpServer.begin();
    if(!tcpServer)
    {
      tcpServer.end();
      return false;
    }
    tcpServer.setNoDelay(false);
  }

  WiFiClient incoming = tcpServer.accept();
  if(incoming)
  {
    if(tcpIsConnected())
      incoming.stop(); // One controller at a time.
    else
    {
      tcpClient = incoming;
      tcpInputResetPending = true;
    }
  }

  return remoteLoop(tcpClient, remoteTCPState, tcpReadInput,
                    tcpMode == TCP_KENWOOD ? RemoteProtocol::Kenwood : RemoteProtocol::Adhoc);
}

bool tcpConsumeAbortPending(uint8_t tcpMode)
{
  return tcpSelectMode(tcpMode) && remoteConsumeAbortPending(remoteTCPState, tcpReadInput,
    tcpMode == TCP_KENWOOD ? RemoteProtocol::Kenwood : RemoteProtocol::Adhoc);
}
