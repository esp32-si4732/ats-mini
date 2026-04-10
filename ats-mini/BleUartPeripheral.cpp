#include "BleUartPeripheral.h"

void BleUartPeripheral::configureDefaults()
{
  BLEDevice::setMTU(517);
  ble_gap_set_prefered_default_le_phy(BLE_GAP_LE_PHY_ANY_MASK, BLE_GAP_LE_PHY_ANY_MASK);
  ble_gap_write_sugg_def_data_len(251, (251 + 14) * 8);
}

void BleUartPeripheral::createServices()
{
  BLEServer* currentServer = server();
  if (currentServer == nullptr) return;

  service = currentServer->createService(UART_SERVICE_UUID);
  txch = service->createCharacteristic(UART_CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  txch->setCallbacks(this);
  rxch = service->createCharacteristic(UART_CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE_NR);
  rxch->setCallbacks(this);
  service->start();
}

void BleUartPeripheral::destroyServices()
{
  BLEServer* currentServer = server();
  if ((currentServer == nullptr) || (service == nullptr)) return;

  service->stop();

  if (rxch)
  {
    service->removeCharacteristic(rxch, true);
    rxch = nullptr;
  }

  if (txch)
  {
    service->removeCharacteristic(txch, true);
    txch = nullptr;
  }

  currentServer->removeService(service);
  service = nullptr;
}

void BleUartPeripheral::configureAdvertising(BLEAdvertising& advertising)
{
  advertising.addServiceUUID(UART_SERVICE_UUID);
}

void BleUartPeripheral::onConnect(BLEServer* server, ble_gap_conn_desc* desc)
{
  ble_gap_set_prefered_le_phy(desc->conn_handle, BLE_GAP_LE_PHY_ANY_MASK, BLE_GAP_LE_PHY_ANY_MASK, BLE_GAP_LE_PHY_CODED_ANY);
  ble_gap_set_data_len(desc->conn_handle, 251, (251 + 14) * 8);
  server->updateConnParams(desc->conn_handle, 6, 12, 0, 200);
}

void BleUartPeripheral::onDisconnect(BLEServer* server, ble_gap_conn_desc* desc)
{
  dataConsumed.release();
  BlePeripheral::onDisconnect(server, desc);
}

void BleUartPeripheral::onWrite(BLECharacteristic* characteristic, ble_gap_conn_desc* desc)
{
  if (characteristic != rxch) return;

  dataConsumed.acquire();
  incomingPacket = characteristic->getValue();
  unreadByteCount = incomingPacket.length();
}

void BleUartPeripheral::onStatus(BLECharacteristic* characteristic, Status status, uint32_t code)
{
}

int BleUartPeripheral::available()
{
  return unreadByteCount;
}

int BleUartPeripheral::peek()
{
  if (unreadByteCount > 0)
  {
    size_t index = incomingPacket.length() - unreadByteCount;
    return incomingPacket[index];
  }
  return -1;
}

int BleUartPeripheral::read()
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

void BleUartPeripheral::flush()
{
}

size_t BleUartPeripheral::write(const uint8_t* data, size_t size)
{
  if (txch == nullptr) return 0;

  size_t chunkSize = BLEDevice::getMTU();
  size_t remainingByteCount = size;
  while (remainingByteCount >= chunkSize)
  {
    delay(20);
    txch->setValue(data, chunkSize);
    txch->notify();
    data += chunkSize;
    remainingByteCount -= chunkSize;
  }
  if (remainingByteCount > 0)
  {
    delay(20);
    txch->setValue(data, remainingByteCount);
    txch->notify();
  }
  return size;
}

size_t BleUartPeripheral::write(uint8_t byte)
{
  return write(&byte, 1);
}

size_t BleUartPeripheral::print(std::string str)
{
  return write((const uint8_t*)str.data(), str.length());
}

size_t BleUartPeripheral::printf(const char* format, ...)
{
  char dummy;
  va_list args;
  va_start(args, format);
  int requiredSize = vsnprintf(&dummy, 1, format, args);
  va_end(args);
  if (requiredSize == 0)
  {
    return write((uint8_t*)&dummy, 1);
  }
  else if (requiredSize > 0)
  {
    char* buffer = (char*)malloc(requiredSize + 1);
    if (buffer)
    {
      va_start(args, format);
      int result = vsnprintf(buffer, requiredSize + 1, format, args);
      va_end(args);
      if ((result >= 0) && (result <= requiredSize))
      {
        size_t writtenBytesCount = write((uint8_t*)buffer, result);
        free(buffer);
        return writtenBytesCount;
      }
      free(buffer);
    }
  }
  return 0;
}
