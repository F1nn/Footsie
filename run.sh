#!/bin/bash
set -euo pipefail

ESP_IDF_EXPORT_SH="${ESP_IDF_EXPORT_SH:-$HOME/esp/esp-idf/export.sh}"
PORT="${PORT:-/dev/ttyACM0}"

if [ -n "${IDF_PATH:-}" ] && command -v idf.py >/dev/null 2>&1; then
    echo "ESP-IDF environment is initialized"
else
    if [ ! -f "$ESP_IDF_EXPORT_SH" ]; then
        echo "ESP-IDF environment is NOT initialized and export script was not found at $ESP_IDF_EXPORT_SH" >&2
        exit 1
    fi

    echo "ESP-IDF environment is NOT initialized. Running $ESP_IDF_EXPORT_SH"
    . "$ESP_IDF_EXPORT_SH"
fi

idf.py build && idf.py -p "$PORT" flash monitor

