#ifndef BLE_CENTRAL_H
#define BLE_CENTRAL_H

#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEScan.h>
#include <BLESecurity.h>

#define BLE_SCAN_DURATION 10
#define MAX_SCAN_ATTEMPTS 3

class BleCentral : protected BLEClientCallbacks, protected BLEAdvertisedDeviceCallbacks {
public:
  BleCentral() = default;
  virtual ~BleCentral();

  void begin(const char* deviceName);
  void end();
  void loop();

  bool isStarted() const;
  bool isConnected() const;
  bool isConnectPending() const;
  const char* peerName() const;

protected:
  virtual void configureSecurity() {}
  virtual void configureScan(BLEScan& scan) {}
  virtual void configureClient() {}
  virtual bool acceptsAdvertisement(BLEAdvertisedDevice& device) = 0;
  virtual bool setupConnectedPeer() = 0;
  virtual void resetConnectedPeerState() = 0;

  virtual void onConnectFailed() {}
  virtual void onScanStart() {}

  BLEClient* client() const;

  bool beginScan(uint32_t seconds = BLE_SCAN_DURATION);
  void stopScan();

  uint8_t scanAttempts = 0;
  uint32_t scanDuration = BLE_SCAN_DURATION;

  void onDisconnect(BLEClient* client) override;
  void onResult(BLEAdvertisedDevice advertisedDevice) override;

private:
  enum class State : uint8_t {
    Idle,
    Started,
    PendingScan,
    Scanning,
    PendingConnect,
    Connecting,
    Connected,
    Disconnecting,
    Stopping,
  };

  enum class ConnectResult : uint8_t {
    Connected,
    RetryScan,
    WaitForDisconnect,
  };

  static void scanCompleteCallback(BLEScanResults results);
  void enterIdle();
  void enterScanning(uint32_t seconds = BLE_SCAN_DURATION);
  void recoverFromDisconnect();
  void handleScanComplete(BLEScanResults& results);
  ConnectResult connectToPeer();
  void clearPeer();
  void disconnectClient(bool wait = false);

  BLEClient* client_ = nullptr;
  BLEAdvertisedDevice* peer_ = nullptr;
  String peerName_;
  State state_ = State::Idle;

  static BleCentral* activeScanner;
};

#endif
