#include <map>
#include <string>

// BLEDevice owns a global client pointer internally. When we explicitly destroy the
// current central client to release the remote tree, clear the singleton as well so
// the wrapper does not retain a dangling pointer.
#define private public
#include <BLEDevice.h>
#undef private

#include "BleCentral.h"

BleCentral* BleCentral::activeScanner = nullptr;
static constexpr uint32_t BLE_DISCONNECT_WAIT_MS = 500;

BleCentral::~BleCentral()
{
  clearPeer();
}

void BleCentral::begin(const char* deviceName)
{
  if (started) return;

  BLEDevice::init(deviceName);
  configureSecurity();
  clearPeer();
  scanAttempts = 0;
  pendingAction_ = PendingAction::None;
  scanActive_ = false;
  started = true;
  startScan();
}

void BleCentral::end()
{
  if (!started) return;

  started = false;
  pendingAction_ = PendingAction::None;
  scanAttempts = 0;
  stopScan();
  resetPeerState();
  clearPeer();
  destroyClient(true);
}

void BleCentral::loop()
{
  if (!started || pendingAction_ == PendingAction::None) return;
  if (isScanning() || isConnected()) return;

  PendingAction action = pendingAction_;
  pendingAction_ = PendingAction::None;
  switch (action)
  {
    case PendingAction::Scan:
      startScan(scanDuration);
      return;

    case PendingAction::Connect:
    {
      ConnectResult result = connectToPeer();
      switch (result)
      {
        case ConnectResult::Connected:
          return;

        case ConnectResult::RetryScan:
          clearPeer();
          onConnectFailed();
          pendingAction_ = PendingAction::Scan;
          return;

        case ConnectResult::WaitForDisconnect:
          onConnectFailed();
          return;
      }
    }

    case PendingAction::None:
      return;
  }
}

bool BleCentral::isStarted() const
{
  return started;
}

bool BleCentral::isConnected() const
{
  return client_ && client_->isConnected();
}

bool BleCentral::isScanning() const
{
  return started && scanActive_;
}

bool BleCentral::isConnectPending() const
{
  return pendingAction_ == PendingAction::Connect;
}

const char* BleCentral::peerName() const
{
  return peerName_.length() ? peerName_.c_str() : nullptr;
}

BLEClient* BleCentral::client() const
{
  return client_;
}

BLEAdvertisedDevice* BleCentral::peer() const
{
  return peer_;
}

void BleCentral::startScan(uint32_t seconds)
{
  if (!started) return;
  if (isScanning() || isConnected()) return;
  if (MAX_SCAN_ATTEMPTS && scanAttempts >= MAX_SCAN_ATTEMPTS) return;
  if (!BLEDevice::getInitialized()) return;

  BLEScan* scan = BLEDevice::getScan();
  if (scan == nullptr) return;
  scan->setAdvertisedDeviceCallbacks(this);
  configureScan(*scan);
  scanDuration = seconds;
  ++scanAttempts;
  onScanStart();
  activeScanner = this;
  scanActive_ = false;

  if (!scan->start(seconds, scanCompleteCallback, false))
  {
    if (activeScanner == this)
      activeScanner = nullptr;
    return;
  }

  scanActive_ = true;
}

void BleCentral::stopScan()
{
  scanActive_ = false;
  if (!BLEDevice::getInitialized()) return;

  BLEScan* scan = BLEDevice::getScan();
  if (scan == nullptr) return;
  scan->stop();
  if (activeScanner == this)
    activeScanner = nullptr;
}

void BleCentral::onConnect(BLEClient* client)
{
  (void)client;
}

void BleCentral::onDisconnect(BLEClient* client)
{
  resetPeerState();
  clearPeer();
  scanAttempts = 0;
  pendingAction_ = started ? PendingAction::Scan : PendingAction::None;
  (void)client;
}

void BleCentral::onResult(BLEAdvertisedDevice advertisedDevice)
{
  if (!matches(advertisedDevice)) return;

  if (peer_ != nullptr) return;

  peer_ = new BLEAdvertisedDevice(advertisedDevice);
  peerName_ = peer_->haveName() ? peer_->getName() : "";
  stopScan();
}

BleCentral::ConnectResult BleCentral::connectToPeer()
{
  if (peer_ == nullptr) return ConnectResult::RetryScan;

  // Recreate the client for each peer connection so NimBLE doesn't reuse
  // stale remote service/descriptor caches from the previous device.
  destroyClient();

  client_ = BLEDevice::createClient();
  if (client_ == nullptr)
    return ConnectResult::RetryScan;
  client_->setClientCallbacks(this);

  configureClient();
  if (!client_->connect(peer_))
    return (client_->getConnId() != ESP_GATT_IF_NONE) ? ConnectResult::WaitForDisconnect : ConnectResult::RetryScan;

  scanAttempts = 0;

  if (!discover())
  {
    if (client_->disconnect() != 0 && !client_->isConnected())
      return ConnectResult::RetryScan;
    return ConnectResult::WaitForDisconnect;
  }

  return ConnectResult::Connected;
}

void BleCentral::scanCompleteCallback(BLEScanResults results)
{
  if (activeScanner != nullptr)
    activeScanner->handleScanComplete(results);
}

void BleCentral::handleScanComplete(BLEScanResults& results)
{
  scanActive_ = false;
  if (activeScanner == this)
    activeScanner = nullptr;

  onScanComplete(results);

  if (!started || isConnected()) return;

  pendingAction_ = (peer_ != nullptr) ? PendingAction::Connect : PendingAction::Scan;
}

void BleCentral::clearPeer()
{
  delete peer_;
  peer_ = nullptr;
  peerName_ = "";
}

void BleCentral::destroyClient(bool disconnect)
{
  if (client_ == nullptr) return;

  if (disconnect && client_->isConnected())
  {
    client_->disconnect();
    uint32_t disconnectStart = millis();
    while (client_->isConnected() && ((uint32_t)(millis() - disconnectStart) < BLE_DISCONNECT_WAIT_MS))
      delay(10);
  }

  if (BLEDevice::m_pClient == client_)
    BLEDevice::m_pClient = nullptr;

  delete client_;
  client_ = nullptr;
}
