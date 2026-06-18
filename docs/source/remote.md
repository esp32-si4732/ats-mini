# Remote control

Remote control has two parts:

* **Transport** - the connection used to reach the receiver.
* **Protocol** - the set of commands or button events carried over that connection.

The receiver currently supports two transports: **USB Serial** and **Bluetooth Low Energy**. It also supports two protocols: the **Ad hoc protocol** and the **Bluetooth HID protocol**.

## Transports

### USB Serial

Enable `Settings -> USB Port -> Ad hoc` to use USB Serial with the ad hoc protocol.

Use [PuTTY](https://www.chiark.greenend.org.uk/~sgtatham/putty/latest.html) or Picocom to connect to the serial port. Alternatively, open the following web terminal in Google Chrome: <https://www.serialterminal.com/>.
[ESP32 documentation](https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32/get-started/establish-serial-connection.html#verify-serial-connection) notes that the default serial settings are 115200 8N1, though 9600 8N1 may be a bit more reliable.

### Bluetooth LE

Enable `Settings -> Bluetooth` to use Bluetooth LE in one of these modes:

* **Ad hoc** - uses the same remote-control protocol as USB Serial, but over BLE.
* **HID** - lets the receiver connect to supported Bluetooth remotes and keyboards.

Bluetooth support is experimental and may be unstable.

#### Local serial port over BLE

If you prefer using a normal local serial port, you can bridge the receiver's BLE Ad hoc mode to one with the [ble-serial](https://github.com/Jakeler/ble-serial/) utility.

First install `uv` <https://docs.astral.sh/uv/getting-started/installation/>

The following example commands are primarily for **Linux** and **macOS**:

1. Enable `Settings -> Bluetooth -> Ad hoc` on the receiver.
2. Scan for the receiver over BLE:

```shell
uv tool run --from ble-serial ble-scan -s 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
```

Example output:

```text
Started general BLE scan

XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX (rssi=-69): ATS-Mini

Finished general BLE scan
```

3. Start the bridge and replace the device id with the one from your scan result:

```shell
uv tool run --from ble-serial ble-serial -d XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
```

This creates a local serial port at `/tmp/ttyBLE`.

4. Open it with your terminal program:

```shell
picocom /tmp/ttyBLE -b115200
```

You can then use the same ad hoc commands documented below, but through the BLE-backed local serial port instead of USB serial.

On Windows this usually takes a few extra steps to expose the BLE link as a usable COM port. See the [ble-serial](https://github.com/Jakeler/ble-serial/) project for platform-specific details.

## Protocols

### Ad hoc protocol

The ad hoc protocol is the main remote-control protocol. It can be used over:

* **USB Serial**
* **Bluetooth LE** in `Settings -> Bluetooth -> Ad hoc` mode

#### Commands

| Button       | Function            | Comments                                                                                         |
|--------------|---------------------|--------------------------------------------------------------------------------------------------|
| <kbd>R</kbd> | Rotate Encoder Up   | Tune the frequency up, scroll the menu, etc                                                      |
| <kbd>r</kbd> | Rotate Encoder Down | Tune the frequency down, scroll the menu, etc                                                    |
| <kbd>e</kbd> | Encoder Click       | The <kbd>e</kbd> emulates a single click and can not be used for preferences reset or long press |
| <kbd>E</kbd> | Encoder Short Press | The <kbd>E</kbd> emulates a short press (>0.5 sec)                                               |
| <kbd>V</kbd> | Volume Up           |                                                                                                  |
| <kbd>v</kbd> | Volume Down         |                                                                                                  |
| <kbd>B</kbd> | Next Band           |                                                                                                  |
| <kbd>b</kbd> | Previous Band       |                                                                                                  |
| <kbd>M</kbd> | Next Mode           | Next modulation                                                                                  |
| <kbd>m</kbd> | Previous Mode       | Previous modulation                                                                              |
| <kbd>S</kbd> | Next step           |                                                                                                  |
| <kbd>s</kbd> | Previous step       |                                                                                                  |
| <kbd>W</kbd> | Next Bandwidth      |                                                                                                  |
| <kbd>w</kbd> | Previous Bandwidth  |                                                                                                  |
| <kbd>A</kbd> | AGC/Att Up          | Automatic Gain Control or Attenuator up                                                          |
| <kbd>a</kbd> | AGC/Att Down        | Automatic Gain Control or Attenuator down                                                        |
| <kbd>L</kbd> | Backlight Up        |                                                                                                  |
| <kbd>l</kbd> | Backlight Down      |                                                                                                  |
| <kbd>I</kbd> | Calibration Up      |                                                                                                  |
| <kbd>i</kbd> | Calibration Down    |                                                                                                  |
| <kbd>O</kbd> | Sleep On            |                                                                                                  |
| <kbd>o</kbd> | Sleep Off           |                                                                                                  |
| <kbd>t</kbd> | Toggle Log          | Toggle the receiver monitor (log) on and off                                                     |
| <kbd>C</kbd> | Screenshot          | Capture a screenshot and print it as a BMP image in HEX format                                   |
| <kbd>c</kbd> | Screenshot (binary) | Capture a screenshot as a raw little-endian RGB565 BMP (about half the bytes of `C`)             |
| <kbd>$</kbd> | Show Memory Slots   | Show memory slots in a format suitable for restoring them after the reset                        |
| <kbd>#</kbd> | Set Memory Slot     | Example `#01,VHF,107900000,FM` (slot, band, frequency, mode). Set freq to 0 to clear a slot.     |
| <kbd>F</kbd> | Set Frequency       | Example `F107900000`. Frequency is in Hz and must stay within the current band. In SSB modes, sub-kHz digits set the BFO. |
| <kbd>T</kbd> | Theme Editor        | Toggle the [theme editor](development.md#theme-editor) on and off                                |
| <kbd>@</kbd> | Get Theme           | Print the current color theme                                                                    |
| <kbd>^</kbd> | Set Theme           | Set the current color theme as a list of HEX numbers (effective until a power cycle)             |

```{hint}
To edit/backup/restore the Memory slots, you can open this [web based tool](memory.md) in Google Chrome.
```

#### Monitor output

The following comma separated information is sent out when the monitor (log) mode is enabled:

| Position | Parameter        | Function          | Comments                                           |
|----------|------------------|-------------------|----------------------------------------------------|
| 1        | APP_VERSION      | F/W version       | Example 201, F/W = v2.01                           |
| 2        | currentFrequency | Frequency         | FM = 10 kHz, AM/SSB = 1 kHz                        |
| 3        | currentBFO       | BFO               | SSB = Hz                                           |
| 4        | bandCal          | BFO               | SSB = Hz                                           |
| 5        | bandName         | Band              | See the [bands table](manual.md#bands-table)       |
| 6        | currentMode      | Mode              | FM/LSB/USB/AM                                      |
| 7        | currentStepIdx   | Step              |                                                    |
| 8        | bandwidthIdx     | Bandwidth         |                                                    |
| 9        | agcIdx           | AGC/Attn          |                                                    |
| 10       | volume           | Volume            | 0 to 63 (0 = Mute)                                 |
| 11       | remoteRssi       | RSSI              | 0 to 127 dBuV                                      |
| 12       | remoteSnr        | SNR               | 0 to 127 dB                                        |
| 13       | tuningCapacitor  | Antenna Capacitor | 0 - 6143                                           |
| 14       | remoteVoltage    | ADC average value | Voltage = Value x 1.702 / 1000                     |
| 15       | remoteSeqnum     | Sequence number   | 0 to 255 repeating sequence                        |

In SSB mode, the "Display" frequency (Hz) = (currentFrequency x 1000) + currentBFO

#### Making screenshots

The screenshot function is intended for interface and theme designers, as well as for the documentation writers. There are two commands, both capturing the same 320×170 image; they differ only in how the pixels are encoded on the wire:

| Command      | Encoding       | Size on the wire | Notes                                     |
|--------------|----------------|------------------|-------------------------------------------|
| <kbd>C</kbd> | ASCII hex BMP  | ~218 KB          | Terminal-safe, widest tool support        |
| <kbd>c</kbd> | Raw binary BMP | ~108 KB          | About half of `C`; a standard `.bmp` file |

##### `C` — hex BMP

A quick one-liner for macOS and Linux over the **USB Serial** transport (change the `/dev/cu.usbmodem14401` serial port name as needed):

```shell
echo -n C | socat -T5 stdio /dev/cu.usbmodem14401,echo=0,raw | xxd -r -p > /tmp/screenshot.bmp
```

The `-T5` idle timeout makes `socat` exit a few seconds after the transfer finishes, instead of waiting until you press Ctrl-C. `xxd -r -p` ignores whitespace, so the carriage returns between rows are harmless.

The `C` output format is stable and may be relied upon by tools:

* A leading CRLF, then a single 132-hex-character header line (a 66-byte little-endian BMP header: 14-byte file header + 40-byte `BITMAPINFOHEADER` + 12-byte `BI_BITFIELDS` masks).
* 170 rows of exactly 1280 lowercase hex characters (320 px × 4), bottom-up, each terminated by CRLF.
* Geometry 320×170, 16 bpp RGB565, masks R = `0xF800`, G = `0x07E0`, B = `0x001F`.

##### `c` — binary BMP

`c` writes the same image as a raw little-endian BMP, preceded by an ASCII frame `BMP:<size>\r\n` so a reader can find the start of the binary and pre-allocate. The decoded file is byte-for-byte identical to `xxd`-decoding the `C` output, in half the bytes:

```shell
echo -n c | socat -T5 stdio /dev/cu.usbmodem14401,echo=0,raw | tail -c +$(( $(printf 'BMP:%s\r\n' 0 | wc -c) )) > /tmp/screenshot.bmp
```

(The framing line is short; the simplest portable approach is to read the stream in a small script, split on the first `\r\n`, and write the rest to a `.bmp` file.)

```{note}
On the LILYGO T-Embed SI473X variant the panel uses BGR colour order, so the red and blue channels are swapped in both formats (this matches the existing `C` behaviour and is not specific to `c`).
```

```{note}
Screenshots over Bluetooth LE work but are slow because the link runs at roughly 2 KB/s; prefer the `c` binary format, or use the USB Serial connection. The raw `socat` one-liners above are written for the USB Serial transport.
```

### Bluetooth HID protocol

The Bluetooth HID protocol is available only over **Bluetooth LE** in `Settings -> Bluetooth -> HID` mode.

In this mode the receiver looks for supported BLE HID devices and maps their key events to receiver actions. A list of supported devices is available in [Accessories: Bluetooth LE input devices](accessories.md#bluetooth-le-input-devices). At the moment, support is limited to the specific HID devices recognized by the firmware.

(key-bindings)=
#### Key bindings

| Bluetooth HID key event | Receiver action               | Comments                                                        |
|-------------------------|-------------------------------|-----------------------------------------------------------------|
| Volume Increment        | Rotate encoder up             | Same effect as turning the encoder clockwise                    |
| Volume Decrement        | Rotate encoder down           | Same effect as turning the encoder counter-clockwise            |
| Next Track              | Press and rotate encoder up   | Uses the receiver's press-and-rotate gesture                    |
| Previous Track          | Press and rotate encoder down | Uses the receiver's press-and-rotate gesture                    |
| Play/Pause single click | Encoder click                 |                                                                 |
| Play/Pause double click | Encoder short press           | Only available on devices that do not report the click duration |
| Play/Pause short press  | Encoder short press           | Only available on devices that report the click duration        |

The press-and-rotate mapping is useful for actions that already depend on that gesture, such as direct frequency input mode and fine tuning in Seek mode.

## Community software

The following community projects may be useful if you want a richer remote-control interface:

* [ATS Mini Controller](https://atsminiradio.com/) - a browser-based controller with on-screen controls, a terminal, screenshot capture, and memory-slot tools. Introduced in [Issue #128](https://github.com/esp32-si4732/ats-mini/issues/128).
* [ATS Mini Companion](https://github.com/rodillo69/ATS-Mini-Companion) - an Android app for controlling the receiver over USB or Bluetooth LE. Introduced in [Discussion #264](https://github.com/esp32-si4732/ats-mini/discussions/264). The custom firmware is no longer required.
* [Mini-Radio Control GUI](https://github.com/Kabuse/Mini-Radio-Control) - a Python desktop GUI for serial control with real-time status display and related tools. Introduced in [Discussion #101](https://github.com/esp32-si4732/ats-mini/discussions/101).
* [Mini-Radio-Control](https://github.com/dustinsterk/Mini-Radio-Control) - another community remote-control project hosted on GitHub. Introduced in [Discussion #101](https://github.com/esp32-si4732/ats-mini/discussions/101#discussioncomment-13848383).
* [ATS-MINI-Remote](https://github.com/Spectral-Source/ATS-MINI-Remote) - a web-based remote control with a Flask backend and a responsive interface for controlling the receiver from devices on the local network. Introduced in [Discussion #101](https://github.com/esp32-si4732/ats-mini/discussions/101#discussioncomment-14549132).
