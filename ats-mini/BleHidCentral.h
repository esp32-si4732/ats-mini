#ifndef BLE_HID_CENTRAL_H
#define BLE_HID_CENTRAL_H

#include "BleCentral.h"

struct BleKnobInput {
  int16_t rotation = 0;
  bool clicked = false;
  bool shortPressed = false;
};

class BleHidCentral : public BleCentral {
public:
  BleHidCentral() = default;

  bool available() const;
  bool read(BleKnobInput& input);
  bool isPressed() const;

protected:
  void configureSecurity() override;
  void configureScan(BLEScan& scan) override;
  void configureClient() override;
  bool matches(BLEAdvertisedDevice& device) override;
  bool discover() override;
  void resetPeerState() override;
  void onScanStart() override;

private:
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

  void handleInputReport(const uint8_t* data, size_t length);
  void holdVirtualPush();

  BleKnobInput pendingInput{};
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
