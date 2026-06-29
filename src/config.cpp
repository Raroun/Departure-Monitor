#include "config.h"

// Filter fuer Haltestelle 1: Wesselstrasse
// Nur diese Linien anzeigen:
const char* STOP1_LINES[] = {"482", "E482"};
const size_t STOP1_LINES_COUNT = sizeof(STOP1_LINES) / sizeof(STOP1_LINES[0]);

// Richtungen ausschliessen, die diesen Text enthalten (z. B. "Dortmund"):
const char* STOP1_EXCLUDE_DIRECTIONS[] = {"Dortmund"};
const size_t STOP1_EXCLUDE_DIRECTIONS_COUNT = sizeof(STOP1_EXCLUDE_DIRECTIONS) / sizeof(STOP1_EXCLUDE_DIRECTIONS[0]);

// Filter fuer Haltestelle 2: Castrop-Rauxel Hbf
const char* STOP2_LINES[] = {"S2", "RE3", "RB32"};
const size_t STOP2_LINES_COUNT = sizeof(STOP2_LINES) / sizeof(STOP2_LINES[0]);

// Nur Abfahrten in Richtungen, die einen dieser Texte enthalten:
// Hinweis: RB32 Richtung Duisburg wird von der VRR-API aktuell teilweise
// als "Oberhausen Hbf" gefuehrt, daher zusaetzlich "Oberhausen" erlauben.
const char* STOP2_DIRECTIONS[] = {"Essen", "Düsseldorf", "Duisburg", "Oberhausen"};
const size_t STOP2_DIRECTIONS_COUNT = sizeof(STOP2_DIRECTIONS) / sizeof(STOP2_DIRECTIONS[0]);
