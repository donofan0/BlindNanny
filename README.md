# BlindNanny

An ESP32-based DIY smart blind, controlled over MQTT (Home Assistant) or a
local web UI. Drives one or two TMC2209 stepper motors (left / right blind)
and can auto-track the sun to block glare.

## Features

- Local web UI (HTTP Basic auth) with a live, draggable blind - two
  independent blinds when a second motor is fitted.
- Home Assistant MQTT auto-discovery with availability, per-side Left/Right
  covers, calibrate button, IP sensor and auto-mode switch.
- Automatic sun-blocking that tracks the sun's real position (DST-proof),
  driving both blinds.
- Auto-homing against a hard stop using TMC2209 StallGuard, with per-motor
  direction invert for mirrored/reversed installs.
- Positions persisted to flash (no slow re-home on every boot), WiFi
  auto-reconnect, and OTA firmware updates.

## Home Assistant

Set your MQTT broker and web/OTA credentials in `login.hpp` (copy
`login.hpp.example`). On boot the device announces itself via MQTT discovery,
so it appears automatically under **Settings → Devices** as a single
"BlindNanny" device with:

- **Single motor:** one **Cover** – open / close / stop / set position.
- **Two motors:** **Left Blind** and **Right Blind** covers, each driving one
  blind independently (no combined cover – group them in HA if you want a
  single control).
- **Auto Sun-Block** switch, **Calibrate** button, **IP Address** sensor.

The device publishes an MQTT availability (Last-Will) topic, so Home Assistant
shows it **offline** if it drops off the network. Optional MQTT-over-TLS is
available via the `USE_MQTT_TLS` flag in `login.hpp` (set the port to 8883).

MQTT topics live under `home/blinds/blind_<id>/`:

| Topic                    | Direction | Purpose                          |
|--------------------------|-----------|----------------------------------|
| `command`                | in        | `OPEN` / `CLOSE` / `STOP` (both) |
| `set_position`           | in        | 0–100 (100 = open, both)         |
| `left/command`, `left/set_position`   | in | left blind only            |
| `right/command`, `right/set_position` | in | right blind only           |
| `position`, `state`      | out       | reported cover position / state  |
| `availability`           | out       | `online` / `offline` (retained)  |

## Sun tracking

The sun position is computed from UTC using the NOAA solar-position algorithm
(true solar time from longitude + equation of time). It does **not** rely on
the configured timezone/GMT offset, so daylight-saving no longer throws the
tracking off in summer. Only latitude and longitude need to be set.

## Access & updates

The web UI is reachable at `http://blindnanny-<id>.local` (mDNS) or by IP, and
requires the `WEB_USER` / `WEB_PASS` credentials from `login.hpp`.

Firmware can be updated over the air (no USB): push a build with the Arduino
IDE / arduino-cli / PlatformIO `espota` uploader to the device's network port.
Set `OTA_PASSWORD` in `login.hpp` to protect it. Don't push an update while a
calibrate (homing) is running.

## Building

The dev container ships `arduino-cli` and the ESP32 build dependencies. To
compile / verify the firmware:

```bash
tools/compile.sh
```

The first run installs the ESP32 core (pinned to 2.0.17) and the required
libraries (TMCStepper, AccelStepper, PubSubClient, ESPAsyncWebServer,
AsyncTCP), then compiles the sketch.

For an ESP32-C6 (Seeed XIAO) build, uncomment `#define ESP32C6` in `config.hpp`.
