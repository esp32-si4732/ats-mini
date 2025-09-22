# Changelog

The user manual is available at <https://esp32-si4732.github.io/ats-mini/manual.html>. The firmware flashing instructions are available at <https://esp32-si4732.github.io/ats-mini/flash.html>

<!-- towncrier release notes start -->

## 2.33 (2025-09-22)


### Changed

- Adjust gamma for display ID 0x04858552 so the themes look closer to how they were designed (at least Orange now doesn't look like a lemon).

## 2.32 (2025-09-16)


### Removed

- Remove the dynamic CPU frequecy feature introduced in v2.31 (it caused sound artifacts when rotating the encoder). [#244](https://github.com/esp32-si4732/ats-mini/issues/244)
- Do not show the "Add" hint on an empty memory slot to prevent confusion with click vs short press.


### Changed

- Avoid drawing background color when drawing text. This dramatically helps UI customization modding efforts (like setting a background image instead of a plain color, [for example](https://github.com/esp32-si4732/ats-mini/discussions/240)). [#239](https://github.com/esp32-si4732/ats-mini/issues/239)
- Move the Web UI credentials form fields below the Wi-Fi settings. [#241](https://github.com/esp32-si4732/ats-mini/issues/241)


### Fixed

- Fix Wi-Fi connection issue to 2nd or 3rd access point configured on the settings web page. [#244](https://github.com/esp32-si4732/ats-mini/issues/244)

## 2.31 (2025-09-13)


### Removed

- Remove faster tuning in Seek mode on SSB and in Scan mode via press & rotate in favor of the new accelerated encoder control.
- Remove the ENABLE_HOLDOFF compile-time option.


### Added

- Encoder acceleration.
- Encoder click now cancels the EiBi schedule download process.


### Changed

- Reduce the upper CB band limit to 28MHz. [#205](https://github.com/esp32-si4732/ats-mini/issues/205)
- Independent USB/LSB calibration values. WARNING: this change will reset the bands settings. [#220](https://github.com/esp32-si4732/ats-mini/issues/220)
- Render partial frequency numbers on the tuning scale around screen edges. [#235](https://github.com/esp32-si4732/ats-mini/issues/235)
- Disable the Memory menu timeout (it is a surfing mode like Seek or Scan). Short press (0.5 sec) saves/clears a slot, click closes the menu.
- EXPERIMENTAL: overclock the I2C bus to 800kHz (affects Si4732).
- Set CPU freq to 240 MHz on encoder rotation, drop back to 80 MHz after 10 seconds of no activity. This results in snappier UI.


### Fixed

- Fix AVC wrapping to avoid selecting odd AVC values. [#207](https://github.com/esp32-si4732/ats-mini/issues/207)
- Add 100ms delay after Si4732 POWER_ON to fix the "Si4732 not detected" issue [#213](https://github.com/esp32-si4732/ats-mini/issues/213)
- Fix misbehaving squelch when changing bands.

## 2.30 (2025-08-07)


### Added

- Add Scan mode. Press the encoder for 0.5 seconds to rescan, press & rotate to tune using a larger step. The scan process can be aborted by clicking or rotating the encoder.


### Changed

- Switch from EEPROM to Preferences library to store the receiver settings. This change removes some old limitations and enables more flexible settings management. WARNING: upgrading to this firmware version from an older one will reset the settings. Also a forced reset might be required (hold the encoder and power on the receiver). [#94](https://github.com/esp32-si4732/ats-mini/issues/94)
- Mute audio amp during seek action to prevent audible artifacts. [#190](https://github.com/esp32-si4732/ats-mini/issues/190)
- Display "Loading SSB" message in the zoomed menu area.
- Extend the 16m broadcast band a bit to include CRI on 17490.
- Increase the number of memory slots to 99.


### Fixed

- Do not lose SSB sub kHz digits when storing Memory slots. [#109](https://github.com/esp32-si4732/ats-mini/issues/109)
- Restore saved bandwidth.
- Use default step when switching modes or memories.

## 2.28 (2025-07-01)


### Added

- Add UTC+5:30 offset for India (Asia/Kolkata). Users with offsets greater than 5:30 might need to readjust their timezone settings (menu indexes have been shifted).
- Enable PSRAM using different build artifacts for OSPI and QSPI ESP32-S3 modules. For more info see <https://esp32-si4732.github.io/ats-mini/flash.html#firmware-files>.


### Changed

- Much better seek sensitivity (SI4735 library patch by @zhang-chong). [#129](https://github.com/esp32-si4732/ats-mini/issues/129)
- 200kHz FM step now uses odd frequencies (99.1, 99.3, etc). [#161](https://github.com/esp32-si4732/ats-mini/issues/161)

### Fixed

- Fix loud clicks when changing bands/modes on the PCB version without the mute circuit. [#103](https://github.com/esp32-si4732/ats-mini/issues/103)
- Do not shadow station names by zoomed menu in the seek mode. [#157](https://github.com/esp32-si4732/ats-mini/issues/157)

## 2.27 (2025-06-07)


### Added

- Allow connecting to the receiver's web UI using the atsmini.local mDNS name in addition to an IP address. [#145](https://github.com/esp32-si4732/ats-mini/issues/145)


### Changed

- Disable Seek mode (menu) timeout.


### Fixed

- Clear RSSI, SNR, and station name when doing normal Seek. [#146](https://github.com/esp32-si4732/ats-mini/issues/146)
- Fix backwards EiBi seek from 30000kHz.

## 2.26 (2025-06-02)


### Added

- Show DHCP-assigned IP address on the About system screen.


### Fixed

- Fix crash when trying download the EiBi schedule in offline mode. [#132](https://github.com/esp32-si4732/ats-mini/issues/132)
- Fix timeout when connecting to Wi-Fi access points.

## 2.25 (2025-05-31)


### Removed

- Disable EEPROM backup/restore option on the settings web page. If you used this feature to restore the EEPROM and now see strange bugs when switching bands, please reset the receiver settings.


### Fixed

- Fix blinking RDS and static frequency name.

## 2.24 (2025-05-30)


### Added

- EiBi schedule support, see https://esp32-si4732.github.io/ats-mini/manual.html#schedule


### Fixed

- Reapply Squelch after waking up from CPU Sleep mode. [#127](https://github.com/esp32-si4732/ats-mini/issues/127)

## 2.23 (2025-05-26)


### Added

- Ability to select FM de-emphasis setting based on region. [#85](https://github.com/esp32-si4732/ats-mini/issues/85)
- Show MAC-address on the receiver status web page. [#114](https://github.com/esp32-si4732/ats-mini/issues/114)
- Add ALL-CT RDS options for those who prefer precise time over WiFi.
- EEPROM backup/restore via the receiver web interface. Restore only works on compatible firmware versions.
- New optional UI layout with large S-meter and S/N-meter.


### Fixed

- Escape quotes in web form fields (like SSIDs & passwords). [#113](https://github.com/esp32-si4732/ats-mini/issues/113)
- Fix the AVC bug, huge thanks to Dave (G8PTN)! [#117](https://github.com/esp32-si4732/ats-mini/issues/117)

## 2.22 (2025-05-23)


### Removed

- Removed the Mute menu option. Use short press instead while in the volume adjustment mode.


### Added

- Experimental Squelch option based on RSSI threshold. Unlikely to work in SSB mode. To turn it off quickly, short press (>0.5 sec) the encoder button in the Squelch menu mode. [#32](https://github.com/esp32-si4732/ats-mini/issues/32)
- Help screen and system info screen (see `Settings->About`). The help screen is also displayed on first start.


### Changed

- Use short press to delete a memory slot.


### Fixed

- Fix restoring memory slots that belong to the bands with the same names. [#100](https://github.com/esp32-si4732/ats-mini/issues/100)

## 2.21 (2025-05-21)


### Changed

- Make the Wi-Fi icon a bit more lightweight.


### Fixed

- Disable the automatic tuning capacitor. [#97](https://github.com/esp32-si4732/ats-mini/issues/97)
- NTP time synchronization no longer ignores seconds.

## 2.20 (2025-05-18)


### Added

- Direct frequency input mode. Press and rotate the encoder to select the step (digit or "half-digit"), rotate the encoder to adjust the frequency, use short press to align frequency to the current step. To exit the mode, click the encoder or wait for a couple of seconds. [#26](https://github.com/esp32-si4732/ats-mini/issues/26)
- `Settings->UTC Offset` now affects the displayed time (whether it was received from RDS or NTP). [#44](https://github.com/esp32-si4732/ats-mini/issues/44)
- `Settings->Scroll Dir.` option to reverse the menu scroll direction. [#79](https://github.com/esp32-si4732/ats-mini/issues/79)
- Add SNR to the serial console log.
- Stop the automatic seek process by clicking the encoder button (exit by rotating it still works).
- Use press+rotate for manual fine tuning in Seek mode.
- Wi-Fi mode to sync time over NTP, view the receiver status and Memory slots.


### Changed

- Experimental: re-enable automatic antenna capacitor on FM, MW and 160M bands (as it was in 1.06 and earlier), plus reset it when switching between AM bands. [#13](https://github.com/esp32-si4732/ats-mini/issues/13)
- The short press (volume) shortcut is no longer global (now works only in VFO mode) and can be used by menus and other modal modes. [#26](https://github.com/esp32-si4732/ats-mini/issues/26)
- SSB tuning is now aligned to the current step. [#76](https://github.com/esp32-si4732/ats-mini/issues/76)
- Completely silence speaker output on mute. [#78](https://github.com/esp32-si4732/ats-mini/issues/78)
- Decrease RSSI & SNR thresholds in FM/AM seek modes.
- Theme editor can be enabled or disabled via the `T` terminal command, without recompiling the firmware.


### Fixed

- Set seek step according to the current step. [#5](https://github.com/esp32-si4732/ats-mini/issues/5)
- SSB memory slots now store frequencies with 100Hz precision. Serial commands that work with memory slots now expect frequencies in Hz. [#79](https://github.com/esp32-si4732/ats-mini/issues/79)
- Fixed wrong menu titles. [#80](https://github.com/esp32-si4732/ats-mini/issues/80)
- Fix audible clicks when scrolling over empty memory slots.


### Improved Documentation

- Add web-based Memory slot edit/backup/restore tool: <https://esp32-si4732.github.io/ats-mini/memory.html>

## 2.14 (2025-05-06)


### Added

- Zoomed menu mode for accessibility purposes (enabled via Settings->Zoom menu). [#71](https://github.com/esp32-si4732/ats-mini/issues/71)
- Serial commands `$` and `#` to backup and restore memory slots.


### Changed

- Extended 75M and 90M band boundaries.

## 2.13 (2025-05-01)


### Added

- Add Seek menu - automatic scan on AM/FM, faster tuning step on SSB. [#26](https://github.com/esp32-si4732/ats-mini/issues/26)
- CPU sleep mode (light sleep) to reduce the power consumption even more.


### Changed

- Increase seek timeout from 8 seconds to 10 minutes.


### Fixed

- Get rid of the short display blink that shows some visual garbage at power on.

## 2.12 (2025-04-29)


### Removed

- Remove the main menu counter.


### Added

- Added RDS radio text, program type, and PI code. Can be enabled via Settings->RDS. [#9](https://github.com/esp32-si4732/ats-mini/issues/9)
- Memory menu to store favorite stations. Press the encoder on an empty slot to store the current frequency and mode, rotate the encoder to select a slot. WARNING: this firmware version resets the receiver settings. [#56](https://github.com/esp32-si4732/ats-mini/issues/56)
- Add `I` and `i` hotkeys to tweak the calibration value. Add separate BFO and calibration fields to the remote log. [#60](https://github.com/esp32-si4732/ats-mini/issues/60)
- Add 10kHz step for WFM mode
- Add `Settings->Sleep Mode`. Controls whether the encoder is locked or not during sleep. Unlocked mode allows tuning (and setting the volume) with less self-induced noise.


### Changed

- Remote encoder keys changed. Use `R` and `r` to rotate it and `e` to click.


### Fixed

- Get rid of 1M and other AM steps in LSB/USB steps menu [#15](https://github.com/esp32-si4732/ats-mini/issues/15)
- Extend the 25, 31, 41, 49, and 60 meter bands. [#50](https://github.com/esp32-si4732/ats-mini/issues/50)

## 2.11 (2025-04-22)


### Added

- Added named frequencies database with FT8 and SSTV frequencies as samples.
- New Magenta color theme


### Changed

- Make RDS clock optional, move it to the info panel. To enable it, set `Settings->RDS` to `PS+CT`. Also clock is synchronized only once until the receiver or RDS CT is turned on and off (this helps to avoid getting wrong time from incorrectly configured FM stations).  WARNING: this firmware resets the receiver settings! [#39](https://github.com/esp32-si4732/ats-mini/issues/39)
- Added DID/DST registers display to About screen.
- Added RGB color bar to About screen.
- Set the default sleep timeout to 0 (disabled)


### Fixed

- Use BGR color order on the GC9307 display type [#41](https://github.com/esp32-si4732/ats-mini/issues/41)
- Fixed display of CB channels. [#51](https://github.com/esp32-si4732/ats-mini/issues/51)
- SSB calibration wasn't applied when changing bands or modes. Now it is. [#53](https://github.com/esp32-si4732/ats-mini/issues/53)

## 2.10 (2025-04-18)


### Changed

- Move Calibration to the Settings menu
- New bands list: more bands, different names and limits, sorted by modulation type for faster switching. WARNING: this change resets the receiver settings!
- Pixel-wise tuning scale scrolling
- Simplify remote serial output, change encoder keybindings. Use `E` and `e` to simulate the encoder rotation, `p` to push the button.


### Fixed

- Fix the screenshot palette. You can use the following oneliner to make a screenshot: `echo -n C | socat stdio /dev/cu.usbmodem14401,echo=0,raw | xxd -r -p > /tmp/screenshot.bmp` [#40](https://github.com/esp32-si4732/ats-mini/issues/40)
- Autodetect the display type and invert/mirror the picture if needed [#41](https://github.com/esp32-si4732/ats-mini/issues/41)
- Fix SSB band limits checks [#46](https://github.com/esp32-si4732/ats-mini/issues/46)
- Hopefully fix the SSB noise tone issue [#46](https://github.com/esp32-si4732/ats-mini/issues/46)
- The "EEPROM Resetting" screen is not hidden

## 2.00 (2025-04-15)


### Added

- New Space theme.


### Changed

- Major code refactoring: split code into modules, get rid of global variables, etc. Huge thanks to Marat Fayzullin for doing the heavy lifting. [#35](https://github.com/esp32-si4732/ats-mini/issues/35)
- Bump ESP32 Arduino core to 3.2.0
- Disable delayed screen update by default
- Remove the Z timezone marker from RDS clock.

## 1.09 (2025-04-03)


### Added

- + Now aligning frequency to the step when tuning in AM and FM modes. [#30](https://github.com/esp32-si4732/ats-mini/issues/30)
- **RDS Time Synchronization**: Added support for displaying the current time synchronized via RDS (Radio Data System) when tuned to FM stations broadcasting time information. The time is displayed below the battery icon and includes a "Z" suffix to indicate UTC time. The time display is only visible after successful synchronization with an RDS signal. [#34](https://github.com/esp32-si4732/ats-mini/issues/34)


### Changed

- Make the Night color theme less bright, fix theme length

## 1.08 (2025-03-25)


### Added

- Support press+rotate for faster SSB tuning [#4](https://github.com/esp32-si4732/ats-mini/issues/4)
- Add channel information for CB. Support European and Russian channel lists from A to H. [#21](https://github.com/esp32-si4732/ats-mini/issues/21)


### Changed

- The default CB channel has been changed to the most popular frequency in Russia 27135 kHz (C15E). [#21](https://github.com/esp32-si4732/ats-mini/issues/21)
- Remove some delays and extra screen repaints (might or might not make the tuning process a bit faster)


### Fixed

- Fix getLastStep for LW/MW [#18](https://github.com/esp32-si4732/ats-mini/issues/18)
- Fix the volume shortcut bug with active menu [#20](https://github.com/esp32-si4732/ats-mini/issues/20)

## 1.07 (2025-03-21)


### Fixed

- Reenable USB Serial [#12](https://github.com/esp32-si4732/ats-mini/issues/12)
- Fix ANTCAP value readings [#13](https://github.com/esp32-si4732/ats-mini/issues/13)
- Disable automatic antenna capacitor [#13](https://github.com/esp32-si4732/ats-mini/issues/13)

## 1.06 (2025-03-20)


### Changed

- Unmute the audio on volume middle press shortcut [#1](https://github.com/esp32-si4732/ats-mini/issues/1)
- Lock encoder rotation during sleep, [#8](https://github.com/esp32-si4732/ats-mini/issues/8)
- Redraw the battery indicator in iOS style [#11](https://github.com/esp32-si4732/ats-mini/issues/11)
- Print tuning capacitor value to Serial port

## 1.05 (2025-03-16)


### Added

- Add EEPROM write icon
- New color themes: Bluesky, eInk, Pager, Orange, Night, Phosphor
- Support for color themes
- Theme editor. To enable it, recompile the firmware with THEME_EDITOR=1 and connect via USB serial port. Press @ to print the current theme, change it using any text editor (see themes.h for details), then press ! and paste the updated theme (effective until the receiver is powered off). Once you are happy, add the resulting colors to themes.h. Check out a useful color picker as well https://chrishewett.com/blog/true-rgb565-colour-picker/


### Changed

- Add repo URL to the About screen, hide battery


### Fixed

- Fix tuner scale height on MW1 & MW2 bands

## 1.04 (2025-03-12)


### Changed

- Refactor the code to make it more DRY
- Return the Volos Project UI with some changes. The new s-meter also serves as a stereo indicator on FM.

## 1.03 (2025-03-11)


### Added

- Add screenshot feature, new command to toggle serial log. Send "C" over the serial port to make a screenshot, then feed the resulting HEX dump to the "xxd -r -p" command to get a BMP image. Use the "t" command to toggle the serial log.


### Changed

- Center and underline the menu headers. Thanks to R8ADR for the patch


### Fixed

- Fix "P" (button press) serial command

## 1.02 (2025-03-11)


### Added

- * Display timeout has been added (Settings > Sleep), a value of zero disables the timeout
  * Turning the display off/on by long pressing the encoder (2 seconds). This reduces QRM from the display
  * Added "O" and "o" commands (display off and on) to the serial port protocol
- AM/FM station search by scrolling with the encoder pressed (the search can also be stopped by rotating the encoder)
- Pressing the encoder for more than 0.5 seconds triggers volume adjustment (this compensates the Volume menu option no longer being highlighted by default)


### Changed

- * The menu has been reorganized, and the Spare items have been removed
  * A Settings menu has been added, and the display brightness adjustment option has been moved there
  * The position in the main menu and settings menu is remembered (until power is turned off)
  * The startup splash screen has been moved to Settings > About (but the reset of settings by turning on the receiver with the encoder pressed still works)
  * The Seek Up/Down menu items have been removed
- Changed the charging indicator (lightning icon instead of the EXT text)


### Fixed

- Fixed the duplication of the MW2 band name

## 1.01 (2025-03-11)

Identical to 1.01 by Dave (G8PTN), just recompiled for `esp32:esp32@3.1.3` Arduino core.

### Added

- Added "MODE" configuration per band (FM, AM, LSB, USB)


### Changed

- Improved tuning speed by delaying the display updates

## 1.00 (2025-03-11)

Identical to 1.00 by Dave (G8PTN), just recompiled for `esp32:esp32@3.1.3` Arduino core.

### Added

- Added "Brightness" menu option

  * This controls the PWM from 32 to 255 (full on) in steps of steps of 32
  * When the brightness is set lower than 255, PSU or RFI noise may be present
- Added "Calibration" menu option

  This allows the SI4732 reference clock offset to be compensated per band
- Added Automatic Volume Control (AVC) menu option. This allows the maximum audio gain to be adjusted.
- Added GPIO1 (Output) control (0=FM, 1 = AM/SSB)
- Added a REMOTE serial interface for debug control and monitoring
- User interface modified:

  * Removed the frequency scale
  * Set "Volume" as the default adjustment parameter
  * Modifed the S-Meter size and added labels
  * All actions now use a single press of the rotary encoder button, with a 10s timeout
  * Added status bar with indicators for Display and EEPROM write activity
  * Added unit labels for "Step" and "BW"
  * Added SSB tuning step options 10Hz, 25Hz, 50Hz, 0.1k and 0.5k
  * Added background refresh of main screen
- VFO/BFO tuning mechanism added based on Goshante ATS_EX firmware

  * This provides "chuff" free tuning over a 28kHz span (+/- 14kHz)
  * Compile option "BFO_MENU_EN" for debug purposes, manual BFO is not required


### Changed

- Modified FM steps options (50k, 100k, 200k, 1M)
- Modified the audio mute behaviour

  * Previously the rx.setAudioMute() appeared to unmute when changing band
  * The "Mute" option now toggles the volume level between 0 and previous value
- Modified the battery monitoring function

  * Uses set voltages for 25%, 50% and 75% with a configurable hysteresis voltage
  * Added voltage reading to status bar
- Settings for AGC/ATTN, SoftMute and AVC stored in EEPROM per mode

  AGC/ATTN (FM, AM, SSB), SoftMute (AM, SSB), AVC (AM, SSB)


### Fixed

- Fix compilation errors related to ledc* calls

  https://docs.espressif.com/projects/arduino-esp32/en/latest/migration_guides/2.x_to_3.0.html#ledc
