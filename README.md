# ESP32-C3 SuperMin BTC Price Display

A live Bitcoin price ticker built from an ESP32-C3 SuperMin and a tiny 0.96" colour TFT.
Fetches BTC/AUD and BTC/USD from CoinGecko every 60 seconds over WiFi and displays both prices with a trend indicator.

## Hardware

| Part | Description |
|------|-------------|
| ESP32-C3 SuperMin | HW-466AB, USB-C |
| 0.96" TFT display | 80×160 RGB IPS, driver ICS17735S (ST7735S-compatible), 4-SPI, 65K colours |

## Wiring

The display's 8 pins plug directly onto one column of the ESP32-C3 SuperMin — only one jumper wire from BLK to 3V3 needed.

```
Display pin:   GND   VCC   SCL   SDA   RES   DC    CS    BLK
Board pin:     GND   3V3    4     3     2     1     0    3V3
```

| Display | ESP32-C3 SuperMin | Function |
|---------|-------------------|----------|
| GND | GND | Ground |
| VCC | 3V3 | Power |
| SCL | GPIO 4 | SPI clock |
| SDA | GPIO 3 | SPI data (MOSI) |
| RES | GPIO 2 | Hardware reset |
| DC | GPIO 1 | Data / Command |
| CS | GPIO 0 | Chip select |
| BLK | 3V3 | Backlight (always on) |

## Setup

1. Install [PlatformIO](https://platformio.org/) (VS Code extension or CLI).
2. Clone / open this project folder.
3. Edit `src/main.cpp` and set your WiFi credentials:
   ```cpp
   const char* WIFI_SSID = "your-network";
   const char* WIFI_PASS = "your-password";
   ```
4. Build and upload: `pio run --target upload`
5. Open the serial monitor at 115200 baud to see debug output.

## Configuration

| Define | Default | Description |
|--------|---------|-------------|
| `FETCH_INTERVAL_MS` | 60000 | How often to fetch a new price (ms) |
| `WIFI_TIMEOUT_MS` | 10000 | WiFi connection timeout (ms) |

## Display layout

```
┌────────────────────────────────┐
│ BITCOIN                      + │  ← yellow header, trend +/- /= or ! on error
│────────────────────────────────│
│ AUD                            │
│         A$123,456              │  ← green/red/white based on price movement
│ USD                            │
│          $80,000               │
└────────────────────────────────┘
```

- **+** price rose since last fetch
- **-** price fell since last fetch
- **=** price unchanged
- **!** fetch failed (last known prices remain on screen)

## Display orientation

If the image appears rotated, change `setRotation(3)` in `setup()` — try values 0–3.

## Data source

[CoinGecko free API](https://www.coingecko.com/en/api) — no API key required.
Rate limit: ~30 calls/minute on the free tier; fetching once per minute is well within limits.

## Dependencies (managed by PlatformIO)

- `adafruit/Adafruit ST7735 and ST7789 Library`
- `adafruit/Adafruit GFX Library`
- `bblanchon/ArduinoJson`
