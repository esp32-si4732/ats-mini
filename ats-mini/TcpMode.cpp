#include "Common.h"
#include "Remote.h"
#include "TcpMode.h"

#include <WiFi.h>
#include <lwip/sockets.h>

static WiFiServer tcpServer(TCP_CONTROL_PORT, 1);
static WiFiClient tcpClient;
static RemoteState remoteTCPState;
static uint32_t lastListenAttempt;

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

void tcpStop()
{
  tcpClient.stop();
  tcpServer.end();
  remoteTCPState = RemoteState{};
}

int tcpLoop(uint8_t tcpMode)
{
  bool networkUp = WiFi.status() == WL_CONNECTED || (WiFi.getMode() & WIFI_AP);
  if(tcpMode != TCP_ADHOC || !networkUp)
  {
    if(tcpServer) tcpStop();
    return 0;
  }

  if(!tcpServer)
  {
    if(millis() - lastListenAttempt < 1000) return 0;
    lastListenAttempt = millis();
    tcpServer.begin();
    if(!tcpServer)
    {
      tcpServer.end();
      return 0;
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
      remoteTCPState = RemoteState{};
      tcpClient = incoming;
    }
  }

  if(!tcpIsConnected()) return 0;
  remoteTickTime(&tcpClient, &remoteTCPState);
  if(!tcpClient.available()) return 0;

  return remoteDoCommand(&tcpClient, &remoteTCPState, tcpClient.read());
}

bool tcpConsumeAbortPending(uint8_t tcpMode)
{
  if(tcpMode != TCP_ADHOC || !tcpClient.available()) return false;
  return tcpClient.read() >= 0;
}
