#ifndef BLE_H
#define BLE_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "host/ble_gap.h"
#include <semaphore>

#include "Remote.h"

#define NORDIC_UART_SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NORDIC_UART_CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NORDIC_UART_CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class NordicUART : public Stream, public BLEServerCallbacks, public BLECharacteristicCallbacks {
private:
  // BLE components
  BLEServer* pServer;
  BLEService* pService;
  BLECharacteristic* pTxCharacteristic;
  BLECharacteristic* pRxCharacteristic;

  // Connection management
  bool started;

  // Data handling
  std::binary_semaphore dataConsumed{1};
  String incomingPacket;
  size_t unreadByteCount = 0;

  // Device attributes
  const char *deviceName;

public:
  NordicUART(const char *name) : deviceName(name) {
    started = false;
    pServer = nullptr;
    pService = nullptr;
    pTxCharacteristic = nullptr;
    pRxCharacteristic = nullptr;
  }

  void start()
  {
    BLEDevice::init(deviceName);
    BLEDevice::setPower(ESP_PWR_LVL_N0); // N12, N9, N6, N3, N0, P3, P6, P9
    BLEDevice::getAdvertising()->setName(deviceName);

    BLEDevice::setMTU(517);
    ble_gap_set_prefered_default_le_phy(BLE_GAP_LE_PHY_ANY_MASK, BLE_GAP_LE_PHY_ANY_MASK);
    ble_gap_write_sugg_def_data_len(251, (251 + 14) * 8);

    pServer = BLEDevice::getServer();
    if (pServer == nullptr)
      pServer = BLEDevice::createServer();

    pServer->setCallbacks(this); // onConnect/onDisconnect
    pServer->getAdvertising()->addServiceUUID(NORDIC_UART_SERVICE_UUID);
    pService = pServer->createService(NORDIC_UART_SERVICE_UUID);
    pTxCharacteristic = pService->createCharacteristic(NORDIC_UART_CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
    pTxCharacteristic->setCallbacks(this); // onSubscribe/onStatus
    pRxCharacteristic = pService->createCharacteristic(NORDIC_UART_CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
    pRxCharacteristic->setCallbacks(this); // onWrite
    pService->start();
    pServer->getAdvertising()->start();
    started = true;
  }

  void stop()
  {
    pServer = BLEDevice::getServer();
    if (pServer)
    {
      pServer->getAdvertising()->stop();
      pService->stop();

      pService->removeCharacteristic(pRxCharacteristic, true);
      pRxCharacteristic = nullptr;

      pService->removeCharacteristic(pTxCharacteristic, true);
      pTxCharacteristic = nullptr;

      pServer->removeService(pService);
      pService = nullptr;
    }
    BLEDevice::deinit(false);
    started = false;
  }

  bool isStarted()
  {
    return started;
  }

  void onConnect(BLEServer *pServer, ble_gap_conn_desc *desc) {
    ble_gap_set_prefered_le_phy(desc->conn_handle, BLE_GAP_LE_PHY_ANY_MASK, BLE_GAP_LE_PHY_ANY_MASK, BLE_GAP_LE_PHY_CODED_ANY);
    ble_gap_set_data_len(desc->conn_handle, 251, (251 + 14) * 8);
    pServer->updateConnParams(desc->conn_handle, 6, 12, 0, 200);
  }

  void onDisconnect(BLEServer *pServer, ble_gap_conn_desc *desc) {
    dataConsumed.release();
    pServer->getAdvertising()->start();
  }

  // FIXME https://github.com/espressif/arduino-esp32/issues/11805
  void onWrite(BLECharacteristic *pCharacteristic, ble_gap_conn_desc *desc)
  {
    if(pCharacteristic == pRxCharacteristic)
    {
      // Wait for previous data to get consumed
      dataConsumed.acquire();

      // Hold data until next read
      incomingPacket = pCharacteristic->getValue();
      unreadByteCount = incomingPacket.length();
    }
  }

  void onStatus(BLECharacteristic *pCharacteristic, Status s, uint32_t code)
  {
    // if(code) Serial.println(code);
  }

  int available()
  {
    return unreadByteCount;
  }

  int peek()
  {
    if (unreadByteCount > 0)
    {
        size_t index = incomingPacket.length() - unreadByteCount;
        return incomingPacket[index];
    }
    return -1;
  }

  int read()
  {
    if (unreadByteCount > 0)
    {
      size_t index = incomingPacket.length() - unreadByteCount;
      int result = incomingPacket[index];
      unreadByteCount--;
      if (unreadByteCount == 0)
        dataConsumed.release();
      return result;
    }
    return -1;
  }

  // It is hard to achieve max throughput witout using delays...
  //
  // https://github.com/nkolban/esp32-snippets/issues/773
  // https://github.com/espressif/arduino-esp32/issues/8413
  // https://github.com/espressif/esp-idf/issues/9097
  // https://github.com/espressif/esp-idf/issues/16889
  // https://github.com/espressif/esp-nimble/issues/75
  // https://github.com/espressif/esp-nimble/issues/106
  // https://github.com/h2zero/esp-nimble-cpp/issues/347
  size_t write(const uint8_t *data, size_t size)
  {
    if (pTxCharacteristic)
    {
      // Data is sent in chunks of MTU size to avoid data loss
      // as each chunk is notified separately
      size_t chunkSize = BLEDevice::getMTU();
      size_t remainingByteCount = size;
      while (remainingByteCount >= chunkSize)
      {
        delay(20);
        pTxCharacteristic->setValue(data, chunkSize);
        pTxCharacteristic->notify();
        data += chunkSize;
        remainingByteCount -= chunkSize;
      }
      if (remainingByteCount > 0)
      {
        delay(20);
        pTxCharacteristic->setValue(data, remainingByteCount);
        pTxCharacteristic->notify();
      }
      return size;
    }
    else
      return 0;
  }

  size_t write(uint8_t byte)
  {
    return write(&byte, 1);
  }

  size_t print(std::string str)
  {
    return write((const uint8_t *)str.data(), str.length());
  }

  size_t printf(const char *format, ...)
  {
    char dummy;
    va_list args;
    va_start(args, format);
    int requiredSize = vsnprintf(&dummy, 1, format, args);
    va_end(args);
    if (requiredSize == 0)
    {
      return write((uint8_t *)&dummy, 1);
    }
    else if (requiredSize > 0)
    {
      char *buffer = (char *)malloc(requiredSize + 1);
      if (buffer)
      {
        va_start(args, format);
        int result = vsnprintf(buffer, requiredSize + 1, format, args);
        va_end(args);
        if ((result >= 0) && (result <= requiredSize))
        {
          size_t writtenBytesCount = write((uint8_t *)buffer, result + 1);
          free(buffer);
          return writtenBytesCount;
        }
        free(buffer);
      }
    }
    return 0;
  }
};

void bleInit(uint8_t bleMode);
void bleStop();
int8_t getBleStatus();
void remoteBLETickTime(Stream* stream, RemoteState* state, uint8_t bleMode);
int bleDoCommand(Stream* stream, RemoteState* state, uint8_t bleMode);
extern NordicUART BLESerial;

#endif
