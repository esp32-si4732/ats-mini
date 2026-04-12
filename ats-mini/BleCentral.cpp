#include "BleCentral.h"

BleCentral* BleCentral::activeScanner = nullptr;

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
  started = true;
  startScan();
}

void BleCentral::end()
{
  if (!started) return;

  started = false;
  stopScan();
  if (client_)
  {
    client_->disconnect();
    delete client_;
    client_ = nullptr;
  }
  resetPeerState();
  delete peer_;
  peer_ = nullptr;
  peerName_ = "";
  pendingConnect = false;
  pendingScanRestart = false;
  BLEDevice::deinit(false);
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
  return started && BLEDevice::getScan()->isScanning();
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
  if (isScanning() || isConnected()) return;
  if (MAX_SCAN_ATTEMPTS && scanAttempts >= MAX_SCAN_ATTEMPTS) return;

  BLEScan* scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(this);
  configureScan(*scan);
  scanDuration = seconds;
  ++scanAttempts;
  onScanStart();
  activeScanner = this;

  if (!scan->start(seconds, scanCompleteCallback, false))
  {
    if (activeScanner == this)
      activeScanner = nullptr;
    return;
  }
}

void BleCentral::stopScan()
{
  BLEScan* scan = BLEDevice::getScan();
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
  pendingScanRestart = true;
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

  if (client_ != nullptr)
  {
    delete client_;
    client_ = nullptr;
  }

  client_ = BLEDevice::createClient();
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
