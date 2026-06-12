# ATS Mini

![](docs/source/_static/esp32-si4732-ui-theme.jpg)

This firmware is for use on the SI4732 (ESP32-S3) Mini/Pocket Receiver

Based on the following sources:

* Volos Projects:    https://github.com/VolosR/TEmbedFMRadio
* PU2CLR, Ricardo:   https://github.com/pu2clr/SI4735
* Ralph Xavier:      https://github.com/ralphxavier/SI4735
* Goshante:          https://github.com/goshante/ats20_ats_ex
* G8PTN, Dave:       https://github.com/G8PTN/ATS_MINI

## Releases

Check out the [Releases](https://github.com/esp32-si4732/ats-mini/releases) page.

## Documentation

The hardware, software and flashing documentation is available at <https://esp32-si4732.github.io/ats-mini/>

## Discuss

* [GitHub Discussions](https://github.com/esp32-si4732/ats-mini/discussions) - the best place for feature requests, observations, sharing, etc.
* [TalkRadio Telegram Chat](https://t.me/talkradio/174172) - informal space to chat in Russian and English.

---

# Fork Features / Novidades deste Fork

This fork adds a full web remote-control interface, an RSSI waterfall, a Morse
(CW) decoder and a frequency-limit override, plus documented serial and HTTP
APIs. The sections below are bilingual: English first, **Português** second.

*Este fork adiciona uma interface web completa de controle remoto, uma cascata
(waterfall) de RSSI, um decodificador de Morse (CW) e a liberação dos limites de
frequência, além de APIs serial e HTTP documentadas. As seções abaixo são
bilíngues: inglês primeiro, **Português** em seguida.*

## Web Control UI / Interface Web de Controle

A modern, responsive, dark "receiver console" served directly by the device.
Connect to the radio's Wi-Fi (AP mode) or join it to your network, then open
**http://atsmini.local/** (mDNS) or the device IP. The Status page is a single
page app that polls `/api/status` once per second and renders everything live:

* High-contrast LED-style **frequency readout** with a separated unit and
  mode-aware formatting (MHz for FM, thousands-separated kHz for AM/SSB).
* Animated **S-meter / SNR / battery** bars and status **badges**
  (Wi-Fi, BLE, RDS, CW, Override, Mute).
* Grouped control panels: **Tuning, Band & Mode, Step & Bandwidth,
  Volume & AGC** with segmented buttons.
* A live **Set Frequency** field that mirrors the current frequency in the
  active unit with a thousands-separator mask (spaces/commas are stripped on
  submit so tuning still works).
* Dynamic **Step quick-select buttons** that reflect the actual steps supported
  by the current band/mode (sourced from the `steps` field of `/api/status`),
  plus a live step readout.
* A live, editable **Memory** page (tune, save current, clear, or manually write
  any slot) and a **Config** page mirroring every on-device setting (brightness,
  theme, UI layout, sleep, RDS mode, FM region, time zone, USB/Bluetooth/Wi-Fi)
  with immediate, persisted changes.

Radio commands from the browser are queued and executed in the main loop to
avoid racing with the receiver (I2C) hardware.

![Web Control UI - Status page](docs/source/_static/web-control.png)

![Web Memory page](docs/source/_static/web-memory.png)

![Web Config page](docs/source/_static/web-config.png)

The layout is fully responsive and works well on a phone:

![Web Control UI on mobile](docs/source/_static/web-control-mobile.png)

*Um "console de receptor" escuro, moderno e responsivo, servido diretamente pelo
rádio. Conecte-se ao Wi-Fi do rádio (modo AP) ou coloque-o na sua rede e abra
**http://atsmini.local/** (mDNS) ou o IP do dispositivo. A página Status é um
aplicativo de página única que consulta `/api/status` uma vez por segundo e
mostra tudo ao vivo: leitura de **frequência** estilo LED com unidade separada e
formatação por modo (MHz para FM, kHz com separador de milhar para AM/SSB);
barras animadas de **S-meter / SNR / bateria** e **selos** de estado
(Wi-Fi, BLE, RDS, CW, Override, Mudo); painéis agrupados de **Sintonia, Banda e
Modo, Passo e Largura de banda, Volume e AGC**; um campo **Definir Frequência**
que acompanha a frequência atual na unidade ativa com máscara de milhar; botões
dinâmicos de **passo rápido** que refletem os passos reais suportados pela
banda/modo atual; uma página de **Memória** editável ao vivo e uma página de
**Configuração** que espelha todas as configurações do dispositivo, aplicadas e
salvas imediatamente. Os comandos enviados pelo navegador são enfileirados e
executados no laço principal para não conflitar com o hardware do receptor.*

## RSSI Waterfall / Cascata de RSSI

A continuous scrolling heatmap of band activity, available both on the device
and in the web UI.

* **On-device:** open **Menu > Waterfall**. Rotating the encoder moves the scan
  center frequency; a button press exits and restores the previous frequency
  and mute state.
* **Web:** the canvas waterfall below the meters is driven by the
  **Waterfall: Off/On** toggle on the Status card. It polls `/api/scan` until a
  scan completes (the `busy` field) before drawing the next row.
* **Mutual exclusion:** while the waterfall is active (on the device, or as the
  web toggle) it owns the radio, so normal tuning/listening is paused. The web
  UI greys out the radio controls and shows a *"Waterfall ativo — rádio
  pausado"* banner.

*Um mapa de calor rolante e contínuo da atividade da banda, disponível tanto no
dispositivo quanto na web. **No dispositivo:** abra **Menu > Waterfall**; girar o
encoder move a frequência central da varredura e um clique sai, restaurando a
frequência e o estado de mudo anteriores. **Na web:** a cascata abaixo dos
medidores é controlada pelo botão **Waterfall: Off/On** e consulta `/api/scan`
até a varredura terminar. **Exclusão mútua:** enquanto a cascata está ativa, ela
assume o rádio, então a sintonia/escuta normal fica pausada; a UI web esmaece os
controles e mostra o aviso "Waterfall ativo — rádio pausado".*

## Morse (CW) Decoder / Decodificador de Morse (CW)

A live Morse decoder with a **pluggable signal source**. It defaults to the
**RSSI/SNR** envelope, which works on stock hardware with no modification. An
**audio (CW)** source samples the demodulated audio on GPIO11 (ADC2) and is meant
for a future hardware mod (wire the audio output to IO11 through an RC low-pass
filter; routed from the factory only on the V4). The audio source is compiled in
with `-DMORSE_AUDIO_INPUT` and otherwise transparently falls back to RSSI/SNR.

Select the source on the device via **Menu > Morse** (Off / RSSI / audio) or from
the web Status page. The decoded text is shown on the device screen and in the
web status (`morseText` in `/api/status`). See
[discussion #267](https://github.com/esp32-si4732/ats-mini/discussions/267).

*Um decodificador de Morse ao vivo com uma **fonte de sinal plugável**. Por
padrão usa o envelope **RSSI/SNR**, que funciona no hardware de fábrica sem
modificação. A fonte **áudio (CW)** amostra o áudio demodulado no GPIO11 (ADC2) e
é destinada a uma futura modificação de hardware (compilada com
`-DMORSE_AUDIO_INPUT`, com fallback automático para RSSI/SNR). Selecione a fonte
em **Menu > Morse** ou pela página web Status; o texto decodificado aparece na
tela e no status web.*

## Frequency Limit Override / Liberação dos Limites de Frequência

Allows tuning beyond the firmware band edges (still within the SI4732 physical
range). It is **locked by default** for normal behavior. Toggle it via
**Settings > Freq Limit** on the device, the checkbox on the web Status page, or
the `P` serial command. When unlocked, the device shows *"Unlocked"* and the web
UI lights the **OVR** badge.

*Permite sintonizar além dos limites de banda do firmware (ainda dentro do
alcance físico do SI4732). Fica **bloqueado por padrão**. Alterne em
**Settings > Freq Limit** no dispositivo, pela caixa de seleção na página web
Status, ou pelo comando serial `P`. Quando liberado, o dispositivo mostra
"Unlocked" e a UI web acende o selo **OVR**.*

## Examples / Exemplos

### Serial / USB remote commands / Comandos remotos Serial / USB

The radio exposes a single-character control protocol over USB serial (enable
**USB Port = Ad hoc** in Config). Lowercase = down/counter-clockwise, uppercase =
up/clockwise. Verified commands (see `ats-mini/Remote.cpp`):

| Key | Action |
| --- | --- |
| `R` / `r` | Encoder clockwise / counter-clockwise (tune) |
| `e` / `E` | Encoder click / short press |
| `B` / `b` | Band up / down |
| `M` / `m` | Mode up / down |
| `S` / `s` | Step up / down |
| `W` / `w` | Bandwidth up / down |
| `A` / `a` | AGC/attenuation up / down |
| `V` / `v` | Volume up / down |
| `L` / `l` | Backlight up / down |
| `O` / `o` | Sleep on / off |
| `P` | Toggle frequency-limit override (lock/unlock) |
| `F<hz>\r` | Set frequency in Hz (honors override), e.g. `F91700000\r` |
| `$` | List stored memories |
| `#<slot>,<band>,<hz>,<mode>\r` | Write a memory slot (freq `0` clears it) |
| `C` | Capture screen as a hex BMP |
| `t` | Toggle periodic status logging |
| `I` / `i` | Calibration up / down |
| `T` / `^` / `@` | Theme editor: toggle / set / get colors |

### Web API examples (curl) / Exemplos de API Web (curl)

All endpoints live in `ats-mini/Network.cpp`. Examples against the live device:

```bash
# Live status (polled by the Status page)
curl http://atsmini.local/api/status

# Nudge a setting / pick a tuning step in kHz
curl 'http://atsmini.local/api/set?step=100'

# Set a direct frequency in Hz (honors the override lock)
curl 'http://atsmini.local/api/freq?hz=91700000'

# Send a raw single-char protocol command (here: tune clockwise)
curl 'http://atsmini.local/api/cmd?c=R'

# Trigger an RSSI scan, then fetch the latest waterfall data
curl 'http://atsmini.local/api/scan?run=1'
curl http://atsmini.local/api/scan

# List memory slots, then save the current frequency into slot 5
curl http://atsmini.local/api/memory
curl 'http://atsmini.local/api/mem?action=save&slot=5'
```

A trimmed real `GET /api/status` response from the device:

```json
{"ver":"F/W: v2.35 Jun 12 2026","band":"VHF","mode":"FM","freq":"91.70",
 "unit":"MHz","freqHz":91700000,"step":"1M","steps":[10,50,100,200,1000],
 "bw":"Auto","agc":0,"vol":36,"muted":false,"rssi":46,"snr":13,"batt":4.67,
 "ip":"192.168.68.101","station":"  WI    ","rt":"TOCA MAIS ALTO ... PX",
 "morse":0,"morseAudio":false,"morseText":"","override":true}
```

`/api/set` also accepts `morse`, `override`, `brt`, `rds`, `region`, `theme`,
`ui`, `zoom`, `scroll`, `sleep`, `sleepmode`, `utc`, `usb`, `ble` and `wifi`.
`/api/mem` supports `action=save|clear|tune|set` (with `band`, `hz`, `mode` for
`set`).

*Todos os endpoints estão em `ats-mini/Network.cpp`. Os exemplos acima funcionam
contra o dispositivo ao vivo: `/api/status` (status ao vivo), `/api/set?step=`
(ajustes/passo), `/api/freq?hz=` (frequência direta, respeita a trava),
`/api/cmd?c=` (comando bruto de um caractere), `/api/scan?run=1` (cascata) e
`/api/memory` + `/api/mem?action=save|clear|tune|set` (memórias).*

### Build / flash / Compilar / gravar

Using the included `ats-mini/Makefile` (profile `esp32s3-ospi`,
[arduino-cli](https://arduino.github.io/arduino-cli/)):

```bash
cd ats-mini

# Compile the firmware
make build

# Compile and flash over USB (adjust the serial port)
make upload PORT=/dev/cu.usbmodem1101
```

*Usando o `ats-mini/Makefile` incluso (perfil `esp32s3-ospi`): `make build` para
compilar e `make upload PORT=/dev/cu.usbmodem1101` para compilar e gravar via USB
(ajuste a porta serial).*
