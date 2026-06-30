#include "config.h"

// Filter for stop 1: Wesselstrasse
// Only show these lines:
const char* STOP1_LINES[] = {"482", "E482"};
const size_t STOP1_LINES_COUNT = sizeof(STOP1_LINES) / sizeof(STOP1_LINES[0]);

// Exclude directions containing this text (e.g. "Dortmund", "Ickern"):
const char* STOP1_EXCLUDE_DIRECTIONS[] = {"Dortmund", "Ickern"};
const size_t STOP1_EXCLUDE_DIRECTIONS_COUNT = sizeof(STOP1_EXCLUDE_DIRECTIONS) / sizeof(STOP1_EXCLUDE_DIRECTIONS[0]);

// Filter for stop 2: Castrop-Rauxel Hbf
const char* STOP2_LINES[] = {"S2", "RE3", "RB32"};
const size_t STOP2_LINES_COUNT = sizeof(STOP2_LINES) / sizeof(STOP2_LINES[0]);

// Only show departures whose direction contains one of these texts:
// Note: RB32 towards Duisburg is currently listed by the VRR API partly as
// "Oberhausen Hbf", therefore also allow "Oberhausen".
const char* STOP2_DIRECTIONS[] = {"Essen", "Düsseldorf", "Duisburg", "Oberhausen"};
const size_t STOP2_DIRECTIONS_COUNT = sizeof(STOP2_DIRECTIONS) / sizeof(STOP2_DIRECTIONS[0]);
