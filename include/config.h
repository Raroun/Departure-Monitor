#pragma once

#include <Arduino.h>

// ============================================
// WLAN Konfiguration
// ============================================
#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_WIFI_SSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

// ============================================
// Abfahrtsmonitor Konfiguration
// ============================================

// API-Typ
//   - API_TRANSPORT_REST : transport.rest APIs (BVG, VBB, DB, ...)
//   - API_VRR_EFA        : VRR EFA rapidJSON (openservice-test.vrr.de)
#define API_TYPE API_VRR_EFA

// Verkehrsverbund / API
#define API_BASE_URL "https://openservice-test.vrr.de/openservice"

// Haltestelle 1: Castrop-Rauxel Wesselstrasse (Busse Richtung Castrop)
#define STOP1_PLACE   "Castrop-Rauxel"
#define STOP1_ID      "Wesselstraße"
#define STOP1_TITLE   "Wesselstr."

// Haltestelle 2: Castrop-Rauxel Hbf (S2 / RE3 Richtung Essen / Duesseldorf)
#define STOP2_PLACE   "Castrop-Rauxel"
#define STOP2_ID      "Hbf"
#define STOP2_TITLE   "Hbf"

// Filter-Konfiguration (definiert in config.cpp)
// STOP1: Wesselstrasse
extern const char* STOP1_LINES[];
extern const size_t STOP1_LINES_COUNT;
extern const char* STOP1_EXCLUDE_DIRECTIONS[];     // Richtungen ausschliessen
extern const size_t STOP1_EXCLUDE_DIRECTIONS_COUNT;

// STOP2: Castrop-Rauxel Hbf
extern const char* STOP2_LINES[];
extern const size_t STOP2_LINES_COUNT;
extern const char* STOP2_DIRECTIONS[];             // Richtungen einschliessen
extern const size_t STOP2_DIRECTIONS_COUNT;

// Maximale Anzahl geladener Abfahrten insgesamt (muss >= API-limit sein,
// damit alle vom Server gelieferten Eintraege verarbeitet werden koennen).
#define MAX_DEPARTURES 55

// Aktualisierungsintervalle in Sekunden
#define DAY_INTERVAL_SEC       120   // alle 2 Min. tagsueber
#define NIGHT_INTERVAL_SEC     7200  // alle 2 Stunden nachts (nur Night-Mode-Screen)
#define NIGHT_START_HOUR       21     // 21:00 Uhr: abends kaum noch Updates noetig
#define NIGHT_END_HOUR         5      // 05:00 Uhr: ab hier wieder alle 2 Minuten updaten

// ============================================
// Display Konfiguration
// ============================================
#define DISPLAY_WIDTH  EPD_WIDTH
#define DISPLAY_HEIGHT EPD_HEIGHT

// API-Typ-Konstanten
#define API_TRANSPORT_REST 0
#define API_VRR_EFA        1
