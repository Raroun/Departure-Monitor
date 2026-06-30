#pragma once

#include <Arduino.h>

struct Departure {
    String line;
    String direction;
    String mode;          // e.g. "bus", "tram", "subway", ...
    String plannedWhen;   // ISO time string
    String when;          // actual / predicted ISO time string
    int delayMinutes;     // delay in minutes
    bool cancelled;
    long minutesUntil;    // minutes until departure (calculated)
};
