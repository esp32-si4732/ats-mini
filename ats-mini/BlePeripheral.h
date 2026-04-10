#ifndef BLE_PERIPHERAL_H
#define BLE_PERIPHERAL_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include "host/ble_gap.h"

class BlePeripheral : protected BLEServerCallbacks {
public:
  BlePeripheral() = default;

  void begin(const char* deviceName);
  void end();

  bool isStarted() const;
  bool isConnected() const;
  uint8_t connectedCount() const;

protected:
  virtual void configureDefaults() {}
  virtual void configureSecurity() {}
  virtual void createServices() {}
  virtual void destroyServices() {}
  virtual void configureAdvertising(BLEAdvertising& advertising) {}

  BLEServer* server() const;
  void onConnect(BLEServer* server, ble_gap_conn_desc* desc) override {}
  void onDisconnect(BLEServer* server, ble_gap_conn_desc* desc) override;

private:
  bool started = false;
};

#endif
