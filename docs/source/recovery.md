# Firmware backup and recovery

## Backup

If you just bought the receiver, want to experiment with different firmwares but want to be able to revert to the stock one, use the following instructions.

1. Install `uv` (Windows, macOS, Linux) <https://docs.astral.sh/uv/getting-started/installation/>
2. Power on the receiver and connect it to your computer via USB
3. Run the following command
   ```
   uvx --from esptool esptool.py --chip esp32s3 --port SERIAL_PORT --baud 921600 read_flash 0x0 ALL original-flash.bin
   ```

Windows-only backup instructions can be found here: <https://github.com/G8PTN/ATS_MINI/issues/24#issuecomment-2766879242>

Alternatively, you can use the [ESPConnect](https://thelastoutpostworkshop.github.io/ESPConnect/) browser-based tool.

## Restore

The following command will restore the firmware from backup:

```shell
uvx --from esptool esptool.py --chip esp32s3 --port SERIAL_PORT --baud 921600 --before default_reset --after hard_reset write_flash  -z --flash_mode keep --flash_freq keep --flash_size keep 0x0 original-flash.bin
```

Alternatively, you can use the [ESPConnect](https://thelastoutpostworkshop.github.io/ESPConnect/) browser-based tool.

## Recovery

Please ensure the receiver has [BOOT and RESET](mods.md#boot-and-reset-buttons) buttons on the PCB.

The recovery process is as follows:

1. Power on the receiver
2. Connect it via USB to a computer
3. Press and hold the BOOT button
4. Press the RESET button
5. Release the BOOT button

The receiver will enter into the recovery mode and you can flash the ESP32-S3 controller as usual.

If your radio only has the BOOT button, use a slightly different recovery sequence:

1. Connect the receiver via USB to a computer
2. Press and hold the BOOT button
3. Power on the receiver
4. Release the BOOT button

The receiver will enter into the recovery mode and you can flash the ESP32-S3 controller as usual.

If your radio doesn't have the BOOT button pads at all, you can short the ESP32-S3 pin 27 (GPIO0) to the ground to simulate the BOOT button press.
