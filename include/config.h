#pragma once

#include <Arduino.h>

// ============================================
// Wi-Fi Configuration
// ============================================
// Credentials are read from include/secrets.h (kept out of Git).
// If secrets.h is missing, fill in the fallback values below for local builds.
#include "secrets.h"

#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_WIFI_SSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

// ============================================
// Departure Monitor Configuration
// ============================================

// API type
//   - API_TRANSPORT_REST : transport.rest APIs (BVG, VBB, DB, ...)
//   - API_VRR_EFA        : VRR EFA rapidJSON (openservice-test.vrr.de)
#define API_TYPE API_VRR_EFA

// Transit authority / API
#define API_BASE_URL "https://openservice-test.vrr.de/openservice"

// Stop 1: Castrop-Rauxel Wesselstrasse (buses towards Castrop)
#define STOP1_PLACE   "Castrop-Rauxel"
#define STOP1_ID      "Wesselstraße"
#define STOP1_TITLE   "Wesselstr."

// Stop 2: Castrop-Rauxel main station (S2 / RE3 towards Essen / Duesseldorf)
#define STOP2_PLACE   "Castrop-Rauxel"
#define STOP2_ID      "Hbf"
#define STOP2_TITLE   "Hbf"

// Filter configuration (defined in config.cpp)
// STOP1: Wesselstrasse
extern const char* STOP1_LINES[];
extern const size_t STOP1_LINES_COUNT;
extern const char* STOP1_EXCLUDE_DIRECTIONS[];     // exclude directions
extern const size_t STOP1_EXCLUDE_DIRECTIONS_COUNT;

// STOP2: Castrop-Rauxel Hbf
extern const char* STOP2_LINES[];
extern const size_t STOP2_LINES_COUNT;
extern const char* STOP2_DIRECTIONS[];             // include directions
extern const size_t STOP2_DIRECTIONS_COUNT;

// Maximum number of loaded departures in total (must be >= API limit so that
// all entries delivered by the server can be processed).
#define MAX_DEPARTURES 55

// Update intervals in seconds
#define DAY_INTERVAL_SEC          180   // every 3 minutes during the day
#define NIGHT_INTERVAL_SEC        7200  // every 2 hours at night (night-mode screen only)
#define LOW_BATTERY_INTERVAL_SEC  600   // every 10 minutes when battery is low (< 20%)
#define NIGHT_START_HOUR          21    // 21:00: hardly any updates needed in the evening
#define NIGHT_END_HOUR            5     // 05:00: from here on update every 2 minutes again

// ============================================
// Display Configuration
// ============================================
#define DISPLAY_WIDTH  EPD_WIDTH
#define DISPLAY_HEIGHT EPD_HEIGHT

// API type constants
#define API_TRANSPORT_REST 0
#define API_VRR_EFA        1
