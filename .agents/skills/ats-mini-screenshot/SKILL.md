---
name: ats-mini-screenshot
description: Capture the ATS Mini display for UI inspection, theme work, or manual screenshots using shell commands.
---

Read `docs/source/remote.md` for connection setup and screen navigation.
Prefer USB unless another transport is requested. On macOS, find the device
with `print -rl -- /dev/cu.*(N)` in zsh.
Use shell commands only; no helper scripts.
For a plain screenshot request, capture the current display without changing settings.

For documentation screenshots, `T` toggles theme editor mode, which keeps
additional indicators visible. See `docs/source/development.md`; toggle it off
after capture if you enabled it.

Capture over USB (`C` returns a hex-encoded BMP), using the actual device path:

```sh
set -o pipefail
capture_dir=$(mktemp -d /tmp/ats-mini-capture.XXXXXX)
printf '%s\n' "$capture_dir"
printf C | socat -t 30 -T 10 - /dev/cu.usbmodem14401,echo=0,raw,ispeed=115200,ospeed=115200 |
  xxd -r -p > "$capture_dir/screenshot.bmp" &&
  convert "$capture_dir/screenshot.bmp" "$capture_dir/screenshot.png" &&
  ect -9 -strip "$capture_dir/screenshot.png"
```

For TCP, replace the USB address and its options with
`TCP4:atsmini.local:60000,connect-timeout=5`.
The pipeline is normally silent; wait for completion before retrying.

Check command results and inspect the PNG at native resolution. If capture is
incomplete or mixed with console output, retry on a quiet connection before
replacing any image.

For manual updates, inspect `docs/source/manual.md` and its existing screenshots.
Replace only the requested images in `docs/source/_static/`, preserving filenames.
Restore settings changed for capture and report the saved paths.
