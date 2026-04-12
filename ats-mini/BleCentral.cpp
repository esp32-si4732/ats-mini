#include "BleCentral.h"

BleCentral* BleCentral::activeScanner = nullptr;
static constexpr uint32_t BLE_DISCONNECT_WAIT_MS = 500;

BleCentral::~BleCentral()
{
  delete peer_;
}

void BleCentral::begin(const char* deviceName)
{
  if (started) return;

  BLEDevice::init(deviceName);
  configureSecurity();
  peerName_ = "";
  scanAttempts = 0;
  pendingConnect = false;
  pendingScanRestart = false;
  scanActive_ = false;
  started = true;
  startScan();
}

void BleCentral::end()
{
  if (!started) return;

  started = false;
  pendingConnect = false;
  pendingScanRestart = false;
  stopScan();
  if (client_)
  {
    client_->disconnect();
    uint32_t disconnectStart = millis();
    while (client_->isConnected() && ((uint32_t)(millis() - disconnectStart) < BLE_DISCONNECT_WAIT_MS))
      delay(10);
  }
  resetPeerState();
  delete peer_;
  peer_ = nullptr;
  peerName_ = "";
}

void BleCentral::loop()
{
  if (!started) return;

  if (pendingScanRestart && !pendingConnect && !isScanning() && !isConnected())
  {
    pendingScanRestart = false;
    startScan(scanDuration);
    return;
  }

  if (pendingConnect)
  {
    pendingConnect = false;
    if (!connectToPeer())
    {
      delete peer_;
      peer_ = nullptr;
      peerName_ = "";
      onConnectFailed();
      startScan(scanDuration);
    }
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
  delete peer_;
  peer_ = nullptr;
  peerName_ = "";
  scanAttempts = 0;
  pendingConnect = false;
  pendingScanRestart = started;
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

bool BleCentral::connectToPeer()
{
  if (peer_ == nullptr) return false;

  // Recreate the client for each peer connection so NimBLE doesn't reuse
  // stale remote service/descriptor caches from the previous device.
  if (client_ != nullptr)
  {
    delete client_;
    client_ = nullptr;
  }

  client_ = BLEDevice::createClient();
  if (client_ == nullptr)
    return false;
  client_->setClientCallbacks(this);

  configureClient();
  if (!client_->connect(peer_))
    return false;

  scanAttempts = 0;

  if (!discover())
  {
    client_->disconnect();
    return false;
  }

  return true;
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

  if (started && !isConnected() && (peer_ != nullptr))
  {
    pendingConnect = true;
    return;
  }

  if (started && !isConnected() && (peer_ == nullptr))
    pendingScanRestart = true;
}
