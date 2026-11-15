# Wallbox knob display – standalone firmware

The same display and knob functions as the ESPHome package, but **without ESPHome and Home Assistant**.
The firmware talks directly to the [evcc](https://evcc.io) REST API and brings its own settings page.

> Status: early version (0.1.0), tested on the hardware with evcc 0.319.

## Features

- Same screen as the ESPHome version: charge ring, PV and charge power, home and vehicle battery,
  charge mode, active phases, animations while charging
- Turn to preselect the charge mode (Off, Smart, Fast), press to send it to evcc; choose which
  modes are on the knob and how they are labelled
- Display timeout; turning or pressing wakes the display without changing the mode
- Local web page for all settings, status and firmware update
- WiFi setup via its own access point with captive portal
- Over-the-air updates via the web page or PlatformIO (ArduinoOTA)
- evcc login with admin password or API key, if your evcc requires authentication

## Flash a ready-made image

The folder [`bin/`](bin/) contains the current firmware as ready-to-flash images:

| File | Use |
|---|---|
| `wallbox-knob-<version>-full.bin` | First install via USB, flash at offset **0x0** (bootloader, partitions and firmware) |
| `wallbox-knob-<version>-ota.bin` | Update via the settings page (Firmware update) |

Without installing anything: open the [ESP web flasher](https://espressif.github.io/esptool-js/) in Chrome or Edge,
connect the display via USB, click *Connect*, choose the `-full.bin` at address `0x0` and click *Program*.
With esptool:

```bash
esptool.py --chip esp32c3 write_flash 0x0 wallbox-knob-0.1.0-full.bin
```

If the board is not detected, hold the knob button while plugging in USB (boot mode).

## Build and flash

Requires [PlatformIO](https://platformio.org) (CLI or the VS Code extension).

```bash
cd standalone
pio run -e wallbox-knob -t upload        # first time via USB
pio run -e wallbox-knob-ota -t upload    # later over the air
```

Every build also writes both images to `bin/` (script `scripts/merge_bin.py`). The version comes
from `FW_VERSION` in `platformio.ini`.

## First start

1. After flashing, the display shows **Setup** and the name of a WiFi network (`WallboxKnob-XXXX`).
2. Connect your phone to it. The settings page opens automatically, otherwise open `http://192.168.4.1`.
3. Enter your WiFi, the evcc address (for example `http://192.168.1.10:7070`) and the loadpoint number, then save.
4. The device restarts, joins your WiFi and is reachable at `http://wallbox-knob.local`.
5. Set an **admin password** on the settings page. Without it, anyone in your network can change the
   settings or upload firmware.

If the saved WiFi cannot be reached at startup, the device opens the setup network again and retries
the saved WiFi every 10 minutes.

## Settings

| Setting | Meaning |
|---|---|
| WiFi SSID, password, hostname | Network; the device is reachable at `http://<hostname>.local` |
| evcc address | URL of evcc incl. port, e.g. `http://evcc.local:7070` |
| Loadpoint number | evcc loadpoint, counted from 1 |
| evcc admin password / API key | Only needed if evcc requires a login to change the mode |
| Poll interval | How often the state is read from evcc (default 2000 ms) |
| Display timeout | Seconds until the display goes dark, 0 = never |
| Brightness | Backlight in % |
| Full scale | kW for a full charge ring (default 22) |
| Charge icon | evcc logo or charging station |
| Modes and texts | Which modes are on the knob, their labels on the display and their evcc names (default `off` / `smart` / `now`; for older evcc versions use `pv` instead of `smart`) |
| Admin password | Protects the settings page (user `admin`) and OTA |

The display fonts contain ASCII only; ä ö ü ß in labels are shown as ae oe ue ss.

## evcc API

| Purpose | Request |
|---|---|
| Read state | `GET /api/state` (only the needed fields are parsed) |
| Set mode | `POST /api/loadpoints/<n>/mode/<off\|smart\|now>` |
| Login | `POST /api/auth/login` with `{"password": "…"}`, cookie `auth` |

Used fields: `pvPower`, `battery.soc` and `battery.power`, and per loadpoint `chargePower`, `charging`,
`connected`, `mode`, `phasesActive`, `vehicleSoc`. Tested with evcc 0.319. Older versions are handled
too: state wrapped in `result`, `batterySoc` / `batteryPower` at the top level, and the modes
`pv` / `minpv` (shown as Smart).

## Code overview

| File | Content |
|---|---|
| `src/main.cpp` | Startup, WiFi, knob handling, timeout, animation |
| `src/display.cpp` | GC9A01 via LovyanGFX, rendered in bands (small RAM footprint) |
| `src/evcc.cpp` | evcc API client in its own task |
| `src/web.cpp` | Settings page, captive portal, firmware upload, ArduinoOTA |
| `src/knob.cpp` | Rotary encoder and button (interrupts) |
| `src/config.cpp` | Settings in flash (NVS) |
