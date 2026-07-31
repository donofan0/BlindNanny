#!/usr/bin/env bash
# Compile / verify the BlindNanny firmware for the ESP32 with arduino-cli.
#
# The first run installs the ESP32 board core and the required libraries
# (a large download); subsequent runs just recompile. Requires arduino-cli
# and pyserial, both provided by the dev-container image.
#
# Usage:  tools/compile.sh
set -euo pipefail

# esp32 core 2.0.x — the ESPAsyncWebServer version used here relies on the
# mbedtls_md5_*_ret API that was removed in core 3.x, so pin 2.0.17.
FQBN="esp32:esp32:esp32"
CORE="esp32:esp32@2.0.17"
ESP32_INDEX="https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"

SKETCH_SRC="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-/tmp/blindnanny-build}"

echo ">> Configuring arduino-cli..."
arduino-cli config init --overwrite >/dev/null 2>&1 || true
arduino-cli config set board_manager.additional_urls "$ESP32_INDEX"

echo ">> Installing ESP32 core and libraries (first run only, large)..."
arduino-cli core update-index
arduino-cli core install "$CORE"
arduino-cli lib install "TMCStepper" "AccelStepper" "PubSubClient" \
                        "ESPAsyncWebServer" "AsyncTCP"

# arduino-cli requires the sketch folder name to match the .ino base name,
# and the project's headers are included with <angle brackets>, so the sketch
# folder must also be on the include path.
echo ">> Staging sketch..."
rm -rf "$BUILD_DIR" && mkdir -p "$BUILD_DIR/BlindNannyV7"
cp "$SKETCH_SRC"/*.ino "$BUILD_DIR/BlindNannyV7/"
cp "$SKETCH_SRC"/*.hpp "$BUILD_DIR/BlindNannyV7/" 2>/dev/null || true
# login.hpp is gitignored; fall back to the checked-in example for a build.
[ -f "$BUILD_DIR/BlindNannyV7/login.hpp" ] || \
  cp "$SKETCH_SRC/login.hpp.example" "$BUILD_DIR/BlindNannyV7/login.hpp"

echo ">> Compiling..."
arduino-cli compile --fqbn "$FQBN" \
  --build-property "compiler.cpp.extra_flags=-I$BUILD_DIR/BlindNannyV7" \
  "$BUILD_DIR/BlindNannyV7/BlindNannyV7.ino"

echo ">> Build OK"
