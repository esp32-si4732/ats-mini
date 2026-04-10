#ifndef BLE_UART_PERIPHERAL_H
#define BLE_UART_PERIPHERAL_H

#include <semaphore>
#include "BlePeripheral.h"

#define UART_SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define UART_CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define UART_CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class BleUartPeripheral : public BlePeripheral, public Stream, protected BLECharacteristicCallbacks {
public:
  BleUartPeripheral() = default;

  int available() override;
  int peek() override;
  int read() override;
  void flush() override;

  size_t write(uint8_t byte) override;
  size_t write(const uint8_t* data, size_t size) override;
  using Print::write;

  size_t print(std::string str);
  size_t printf(const char* format, ...);

protected:
  void configureDefaults() override;
  void createServices() override;
  void destroyServices() override;
  void configureAdvertising(BLEAdvertising& advertising) override;

  void onConnect(BLEServer* server, ble_gap_conn_desc* desc) override;
  void onDisconnect(BLEServer* server, ble_gap_conn_desc* desc) override;

  void onWrite(BLECharacteristic* characteristic, ble_gap_conn_desc* desc) override;
  void onStatus(BLECharacteristic* characteristic, Status status, uint32_t code) override;

private:
  BLEService* service = nullptr;
  BLECharacteristic* txch = nullptr;
  BLECharacteristic* rxch = nullptr;

  std::binary_semaphore dataConsumed{1};
  String incomingPacket;
  size_t unreadByteCount = 0;
};

#endif
