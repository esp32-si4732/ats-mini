#ifndef BLE_HID_CENTRAL_H
#define BLE_HID_CENTRAL_H

#include "BleCentral.h"

#define BLE_SCAN_INTERVAL 100
#define BLE_SCAN_WINDOW 100

struct BleHidState {
  bool isPressed = false;
  int16_t rotation = 0;
  bool wasClicked = false;
  bool wasShortPressed = false;
};

class BleHidCentral : public BleCentral {
public:
  BleHidCentral() = default;

  BleHidState update();

protected:
  void configureSecurity() override;
  void configureScan(BLEScan& scan) override;
  void configureClient() override;
  bool matches(BLEAdvertisedDevice& device) override;
  bool discover() override;
  void resetPeerState() override;
  void onScanStart() override;

private:
  enum class DecoderKind : uint8_t {
    None,
    ConsumerBitfield16,
    ConsumerUsage16,
  };

  static constexpr uint32_t virtualPushHoldMs = 150;
  static constexpr uint32_t playPauseDoubleClickMs = 400;

  class SecurityCallbacks : public BLESecurityCallbacks {
    void onAuthenticationComplete(ble_gap_conn_desc *desc) override { (void)desc; }
  };

  static void notifyCallback(
    BLERemoteCharacteristic* characteristic,
    uint8_t* data,
    size_t length,
    bool isNotify);

  static bool tryMatchMiniKeyboard(BLERemoteService* deviceInfoService, DecoderKind& decoder, uint16_t& reportHandle);
  static bool tryMatchVol20(BLERemoteService* deviceInfoService, DecoderKind& decoder, uint16_t& reportHandle);
  void clearDecoder();
  void handleInputReport(BLERemoteCharacteristic* characteristic, const uint8_t* data, size_t length);
  void holdVirtualPush();

  BleHidState pendingState{};
  DecoderKind decoder_ = DecoderKind::None;
  uint16_t reportHandle_ = 0;
  uint32_t virtualPushUntil = 0;
  uint32_t playPauseClickDeadline = 0;
  bool scanNextPressed = false;
  bool scanPreviousPressed = false;
  bool volumeIncrementPressed = false;
  bool volumeDecrementPressed = false;
  bool playPauseClickPending = false;
  bool playPausePressed = false;
  BLESecurity security;
  SecurityCallbacks securityCallbacks;

  static BleHidCentral* activeInstance;
};

#endif
