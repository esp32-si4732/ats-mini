#include "Common.h"
#include "Remote.h"
#include "TcpMode.h"

#include <WiFi.h>
#include <lwip/sockets.h>

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
  if(tcpClient.available()) return true;

  // The pinned core reports connected after EOF. Check only after draining
  // WiFiClient's RX buffer, so a peer's final commands are still processed.
  uint8_t byte;
  if(recv(tcpClient.fd(), &byte, 1, MSG_PEEK | MSG_DONTWAIT) == 0)
    tcpClient.stop();
  return tcpClient.fd() >= 0;
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
}

bool tcpLoop(uint8_t tcpMode)
{
  bool networkUp = WiFi.status() == WL_CONNECTED || (WiFi.getMode() & WIFI_AP);
  if(tcpMode != TCP_ADHOC || !networkUp)
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

  return remoteLoop(&tcpClient, &remoteTCPState, tcpReadInput);
}

bool tcpConsumeAbortPending(uint8_t tcpMode)
{
  return tcpMode == TCP_ADHOC && remoteConsumeAbortPending(&remoteTCPState, tcpReadInput);
}
