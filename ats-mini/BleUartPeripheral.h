#ifndef BLE_UART_PERIPHERAL_H
#define BLE_UART_PERIPHERAL_H

#include <cbuf.h>
#include "BlePeripheral.h"

#define UART_SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define UART_CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define UART_CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_BUFFER_SIZE             1024
#define BLE_MIN_MTU                 23
#define BLE_MAX_MTU                 517
#define BLE_TX_PAYLOAD_CAP          40
#define BLE_TX_DEFER_MS             5
#define BLE_NOTIFY_RETRY_DELAY_MS   50

enum class BleUartTxNotifyState : uint8_t {
  Idle,
  // NimBLE-Arduino reports a successful notify twice on this stack.
  // The first callback is treated as local acceptance, the second one as
  // completion that lets us release the pending chunk.
  WaitingFirstStatus,
  WaitingSecondStatus,
};

class BleUartPeripheral : public BlePeripheral, public Stream, protected BLECharacteristicCallbacks {
public:
  BleUartPeripheral() = default;

  int available() override;
  int peek() override;
  int read() override;
  void flush() override;
  bool consumeAbortPending();

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
  void onSubscribe(BLECharacteristic* characteristic, ble_gap_conn_desc* desc, uint16_t subValue) override;
  void onStatus(BLECharacteristic* characteristic, Status status, uint32_t code) override;

private:
  bool canSend() const;
  size_t txPayloadSize() const;
  size_t pumpTx();
  void clearPendingTx();
  void resetTxSession();

  BLEService* service = nullptr;
  BLECharacteristic* txCh = nullptr;
  BLECharacteristic* rxCh = nullptr;

  cbuf rxBuf{BLE_BUFFER_SIZE};
  cbuf txBuf{BLE_BUFFER_SIZE};
  volatile bool abortPending = false;
  // Flat notify payload assembled from txBuf, even when the ring wraps.
  uint8_t txChunk[BLE_MAX_MTU - 3];
  // Bytes currently retained for notify/retry outside txBuf.
  size_t txPendingLen = 0;
  uint16_t txConnHandle = BLE_HS_CONN_HANDLE_NONE;
  // Negotiated ATT MTU for the active peer; actual TX uses min(mtu - 3, cap).
  uint16_t txPeerMtu = BLE_MIN_MTU;
  // Delay before starting a fresh burst so small writes can batch together.
  uint32_t txDeferUntilMs = 0;
  // Backoff deadline for retrying the current pending chunk after ENOMEM/EAPP.
  uint32_t txRetryAfterMs = 0;
  // Tracks the two SUCCESS_NOTIFY callbacks seen on this wrapper.
  BleUartTxNotifyState txNotifyState = BleUartTxNotifyState::Idle;
  bool txSubscribed = false;
};

#endif
