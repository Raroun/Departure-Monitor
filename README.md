# Departure Monitor for Lilygo T5 4.7" S3 E-Ink

A battery-powered departure monitor built for the **Lilygo T5 4.7" S3 V2.4** E-Ink display (ESP32-S3).

The device connects to a public transport API over Wi-Fi, fetches upcoming departures for two stops, and displays them on the E-Paper screen. After each update the ESP32 goes into deep sleep to save power. It is designed to hang near the front door so the next bus or train is visible at a glance.

## Features

- Wi-Fi connection with automatic NTP time synchronization
- Support for public transport APIs:
  - `transport.rest` (BVG Berlin, VBB, DB, ...)
  - VRR EFA rapidJSON (e.g. Castrop-Rauxel)
- Two separate stop sections on one screen
  - Top section: bus stop (e.g. Wesselstraße)
  - Bottom section: train station (e.g. Castrop-Rauxel Hbf)
- Per-stop line and direction filters
- Per-line next-departure selection for the train station, so S2 / RE3 / RB32 are always shown
- Displays line, destination, remaining minutes and delay
- Last update time shown top-right
- Day/night mode:
  - Day (05:00–21:00): updates every 3 minutes
  - Night (21:00–05:00): shows a static "Offline" screen and sleeps in 2-hour intervals until morning
- E-Ink display with 4 gray levels (960 × 540 px)
- Deep sleep between updates with Wi-Fi and Bluetooth powered down
- Fits in a custom 3D-printed case

## Project Structure

```
.
├── include/
│   ├── config.h          # Stops, API, intervals
│   ├── secrets.h         # Wi-Fi credentials (not tracked by Git)
│   ├── departure.h       # Departure data structure
│   ├── departure_api.h   # API client
│   ├── display_manager.h # Display wrapper
│   └── wifi_manager.h    # Wi-Fi helper
├── src/
│   ├── main.cpp          # Main flow
│   ├── config.cpp        # Line / direction filters
│   ├── departure_api.cpp
│   ├── display_manager.cpp
│   └── wifi_manager.cpp
├── platformio.ini        # PlatformIO configuration
└── README.md
```

## Requirements

- Python 3
- The project includes PlatformIO in a local virtual environment (`.venv`)
- Lilygo T5 4.7" S3 V2.4 E-Ink board
- USB-C cable

## Configuration

Open `include/config.h` and adjust at least the following values.

### VRR EFA (default, e.g. Castrop-Rauxel)

```cpp
#define API_TYPE       API_VRR_EFA
#define API_BASE_URL   "https://openservice-test.vrr.de/openservice"

#define STOP1_PLACE    "Castrop-Rauxel"
#define STOP1_ID       "Wesselstraße"
#define STOP2_PLACE    "Castrop-Rauxel"
#define STOP2_ID       "Hbf"
```

### transport.rest (e.g. BVG Berlin)

```cpp
#define API_TYPE       API_TRANSPORT_REST
#define API_BASE_URL   "https://v6.bvg.transport.rest"

#define STOP1_ID       "900000100003"
#define STOP2_ID       "..."
```

### Wi-Fi

Create `include/secrets.h` with your credentials (this file is ignored by Git):

```cpp
#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_WIFI_SSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif
```

If `include/secrets.h` is missing, you can also fill in the fallback values directly in `include/config.h`.

### Line and direction filters

Filters are defined in `src/config.cpp`:

```cpp
// Stop 1: Wesselstraße
const char* STOP1_LINES[] = {"482", "E482"};
const char* STOP1_EXCLUDE_DIRECTIONS[] = {"Dortmund", "Ickern"};

// Stop 2: Castrop-Rauxel Hbf
const char* STOP2_LINES[] = {"S2", "RE3", "RB32"};
const char* STOP2_DIRECTIONS[] = {"Essen", "Düsseldorf", "Duisburg", "Oberhausen"};
```

## Build & Flash

From the project directory:

```bash
# Build
.venv/Scripts/pio run

# Upload (connect board via USB-C)
.venv/Scripts/pio run --target upload

# Serial Monitor
.venv/Scripts/pio device monitor
```

## VRR EFA API Notes

- The default `API_BASE_URL` is the **VRR OpenService test server**. It can be used without registration for hobby/test purposes.
- For permanent or public use you should request access to the **production server** by sending a short project description to `opendata-oepnv@vrr.de`.
- The test server URL may change or be restricted without notice.

## Power Saving

- The ESP32 enters deep sleep after every update.
- Wi-Fi and Bluetooth are powered down during sleep.
- The E-Ink screen only consumes power while updating.
- Intervals are configured in `include/config.h`:
  - `DAY_INTERVAL_SEC`: update interval during the day (default 180 s)
  - `NIGHT_INTERVAL_SEC`: sleep interval at night (default 7200 s)
  - `NIGHT_START_HOUR` / `NIGHT_END_HOUR`: night-mode window (default 21:00–05:00)

## Troubleshooting

- **Display stays black:** open the Serial Monitor and check if Wi-Fi and API are connecting.
- **Wi-Fi does not connect:** check SSID and password in `include/secrets.h` (case-sensitive).
- **API returns no data:** verify `STOP_ID` / `STOP_PLACE`. The VRR test API can occasionally be slow or return empty results.
- **Stuck in night mode:** the timezone is set to `CET-1CEST` with automatic DST. Make sure NTP can sync after a reset.

## License

MIT
