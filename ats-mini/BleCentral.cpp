#include "BleCentral.h"

BleCentral* BleCentral::activeScanner = nullptr;
static constexpr uint32_t BLE_DISCONNECT_WAIT_MS = 500;

BleCentral::~BleCentral()
{
  clearPeer();
}

void BleCentral::begin(const char* deviceName)
{
  if (state_ != State::Idle) return;

  BLEDevice::init(deviceName);
  configureSecurity();
  clearPeer();
  scanAttempts = 0;
  state_ = State::Started;
  enterScanning();
}

void BleCentral::end()
{
  if (state_ == State::Idle) return;
  enterIdle();
}

void BleCentral::loop()
{
  switch (state_)
  {
    case State::PendingScan:
      enterScanning(scanDuration);
      return;

    case State::PendingConnect:
    {
      state_ = State::Connecting;
      ConnectResult result = connectToPeer();
      if (state_ != State::Connecting)
        return;

      switch (result)
      {
        case ConnectResult::Connected:
          state_ = State::Connected;
          return;

        case ConnectResult::RetryScan:
          clearPeer();
          onConnectFailed();
          state_ = State::PendingScan;
          return;

        case ConnectResult::WaitForDisconnect:
          onConnectFailed();
          state_ = State::Disconnecting;
          return;
      }
      return;
    }

    case State::Idle:
    case State::Started:
    case State::Disconnecting:
    case State::Scanning:
    case State::Stopping:
    case State::Connecting:
    case State::Connected:
      return;
  }
}

bool BleCentral::isStarted() const
{
  return state_ != State::Idle;
}

bool BleCentral::isConnected() const
{
  return state_ == State::Connected && client_ && client_->isConnected();
}

bool BleCentral::isConnectPending() const
{
  return state_ == State::PendingConnect;
}

const char* BleCentral::peerName() const
{
  return peerName_.length() ? peerName_.c_str() : nullptr;
}

BLEClient* BleCentral::client() const
{
  return client_;
}

bool BleCentral::beginScan(uint32_t seconds)
{
  if (state_ == State::Idle || state_ == State::Stopping) return false;
  if (isConnected()) return false;
  if (MAX_SCAN_ATTEMPTS && scanAttempts >= MAX_SCAN_ATTEMPTS) return false;
  if (!BLEDevice::getInitialized()) return false;

  BLEScan* scan = BLEDevice::getScan();
  if (scan == nullptr) return false;
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
    return false;
  }
  return true;
}

void BleCentral::stopScan()
{
  if (!BLEDevice::getInitialized()) return;

  BLEScan* scan = BLEDevice::getScan();
  if (scan == nullptr) return;
  scan->stop();
  if (activeScanner == this)
    activeScanner = nullptr;
}

void BleCentral::onDisconnect(BLEClient* client)
{
  if (state_ == State::Connected || state_ == State::Connecting || state_ == State::Disconnecting)
    recoverFromDisconnect();
  (void)client;
}

void BleCentral::onResult(BLEAdvertisedDevice advertisedDevice)
{
  if (!acceptsAdvertisement(advertisedDevice)) return;
  if (state_ != State::Scanning) return;

  if (peer_ != nullptr) return;

  peer_ = new BLEAdvertisedDevice(advertisedDevice);
  peerName_ = peer_->haveName() ? peer_->getName() : "";
  stopScan();
}

BleCentral::ConnectResult BleCentral::connectToPeer()
{
  if (peer_ == nullptr) return ConnectResult::RetryScan;

  if (client_ == nullptr)
  {
    client_ = BLEDevice::createClient();
    if (client_ == nullptr)
      return ConnectResult::RetryScan;
  }
  client_->setClientCallbacks(this);

  configureClient();
  if (!client_->connect(peer_))
    return (client_->getConnId() != ESP_GATT_IF_NONE) ? ConnectResult::WaitForDisconnect : ConnectResult::RetryScan;

  // Rebuild the remote tree on each connection so cached services,
  // characteristics, and descriptors do not leak across peers.
  client_->getServices();
  scanAttempts = 0;

  if (!setupConnectedPeer())
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
  if (activeScanner == this)
    activeScanner = nullptr;
  (void)results;

  if (state_ != State::Scanning) return;

  state_ = (peer_ != nullptr) ? State::PendingConnect : State::PendingScan;
}

void BleCentral::enterIdle()
{
  state_ = State::Stopping;
  stopScan();
  disconnectClient(true);
  resetConnectedPeerState();
  clearPeer();
  scanAttempts = 0;
  state_ = State::Idle;
}

void BleCentral::enterScanning(uint32_t seconds)
{
  if (state_ == State::Idle || state_ == State::Stopping) return;

  state_ = State::Scanning;
  if (!beginScan(seconds))
    state_ = State::Started;
}

void BleCentral::recoverFromDisconnect()
{
  resetConnectedPeerState();
  clearPeer();
  scanAttempts = 0;

  if (state_ == State::Stopping || state_ == State::Idle)
    return;

  state_ = State::PendingScan;
}

void BleCentral::clearPeer()
{
  delete peer_;
  peer_ = nullptr;
  peerName_ = "";
}

void BleCentral::disconnectClient(bool wait)
{
  if (client_ == nullptr) return;

  if (wait && client_->isConnected())
  {
    client_->disconnect();
    uint32_t disconnectStart = millis();
    while (client_->isConnected() && ((uint32_t)(millis() - disconnectStart) < BLE_DISCONNECT_WAIT_MS))
      delay(10);
  }
}
