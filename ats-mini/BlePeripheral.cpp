#include "BlePeripheral.h"

void BlePeripheral::begin(const char* deviceName)
{
  if (started) return;

  BLEDevice::init(deviceName);
  BLEDevice::setPower(ESP_PWR_LVL_N0);
  configureDefaults();
  configureSecurity();

  BLEServer* currentServer = server();
  if (currentServer == nullptr)
    currentServer = BLEDevice::createServer();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  currentServer->setCallbacks(this);
  createServices();
  configureAdvertising(*advertising);
  advertising->start();
  started = true;
}

void BlePeripheral::end()
{
  if (!started) return;

  started = false;
  BLEServer* currentServer = server();
  if (currentServer)
    currentServer->getAdvertising()->stop();

  destroyServices();

  BLEDevice::deinit(false);
}

bool BlePeripheral::isStarted() const
{
  return started;
}

bool BlePeripheral::isConnected() const
{
  return connectedCount() > 0;
}

uint8_t BlePeripheral::connectedCount() const
{
  BLEServer* currentServer = server();
  return currentServer ? currentServer->getConnectedCount() : 0;
}

BLEServer* BlePeripheral::server() const
{
  return BLEDevice::getServer();
}

void BlePeripheral::onDisconnect(BLEServer* server, ble_gap_conn_desc* desc)
{
  if (started)
    server->getAdvertising()->start();
}
