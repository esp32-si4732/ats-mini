```{toctree}
---
maxdepth: 1
hidden: true
---
manual.md
flash.md
recovery.md
accessories.md
mods.md
hardware.md
development.md
changelog.md
```

# ESP32-SI4732 Radio Receiver

![](_static/esp32-si4732-ui-theme.jpg)

```{hint}
For a quick start, check out the [User Manual](manual.md) and the [Flashing Instructions](flash.md). The firmware releases are available on [GitHub](https://github.com/esp32-si4732/ats-mini/releases).
```

The receiver was designed by a Chinese engineer. His nickname is Sunnygold and he open sourced both hardware (board, 3d-printed and cnc-machined cases) and software. Initially, he didn't knew that the receiver was sold on AliExpress and became popular on YouTube and other communities. Then another Chinese developer Zooc joined him to help improve the firmware.

This documentation and the corresponding [GitHub org](https://github.com/esp32-si4732) was created by Max Arnold (R9UCL) and is not official in any way. It is just a collection of notes, source code, plus the excellent docs made by Dave (G8PTN). It also documents [my own fork](https://github.com/esp32-si4732/ats-mini) that is heavily based on the alternative G8PTN firmware (see the full [changelog](changelog.md) here).

## Hardware and software

The receiver uses ESP32-S3 and SI4732 and is based on the following projects:

* [FM Radio project LilyGO T-Embed ESP32 S3](https://www.youtube.com/watch?v=bg2Ysrh85Ek) ([source](https://github.com/VolosR/TEmbedFMRadio)) by Volos Projects, that uses the TEA5767 FM radio module
* [ESP32 OLED_ALL_IN_ONE](https://github.com/pu2clr/SI4735/tree/master/examples/SI47XX_06_ESP32/OLED_ALL_IN_ONE) sketch by PU2CLR (Ricardo Caratti) that uses SI4735-D60 or Si4732-A10
* [Port of ESP32 OLED_ALL_IN_ONE](https://github.com/ralphxavier/SI4735/tree/master/Lilygo_T-Display_S3/ALL_IN_ONE_T-Display_S3) by Ralph Xavier to Lilygo T-Display S3 using the [Volos interface](https://github.com/VolosR/TEmbedFMRadio)
* Open source (CC BY-NC-SA 3.0) [hardware and software](https://oshwhub.com/sunnygold/esp32s3-si4732-shou-yin-ji) (based on Ralph Xavier's work licensed under the MIT License) by Sunnygold. A copy of the attached files from oshwhub.com can be found [here](https://github.com/esp32-si4732/esp32-si4732-oshwhub).

An alternative firmware called [ATS Mini](https://github.com/G8PTN/ATS_MINI/) was developed by Dave (G8PTN) based on Ralph Xavier's work and [firmware for ATS-20 by Goshante](https://github.com/goshante/ats20_ats_ex/). He also documented a lot of things.

## Useful links

* [ESP32S3-SI4732 Receiver](https://oshwhub.com/sunnygold/esp32s3-si4732-shou-yin-ji) the official project page in Chinese (also the QQ group 854656412)
* [ESP32-S3 SI4732 pocket multiband receiver](https://xtronic.org/circuit/rf/radio-receiver/esp32-s3-si4732-pocket-multiband-receiver/) blog post by Toni
* [WOW! Micro Pocket SSB receiver!](https://www.youtube.com/watch?v=yCB4Oam5dwI) review by OM0ET
* [Micro Pocket SSB Receiver - NEW FIRMWARE + Hi-Z CIRCUIT Mod](https://www.youtube.com/watch?v=BzrOE9BFpyU) by OM0ET
* [Компактный AM FM SSB приемник из Китая](https://rutube.ru/video/5f95fa89786735d1da0cea30a2101649/) обзор от R3MDG
* [Китайский мини-радиоприемник на ESP32 / Si4732](https://muromdx.ru/articles/radio-receivers/kitajskij-mini-radiopriemnik-na-esp32-si4732) статья на muromdx.ru
* <http://mini-radio.com/> - seems to be created by a Chinese hardware seller, but it is not affiliated with the original creators (Sunnygold and Zooc)
