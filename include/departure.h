#pragma once

#include <Arduino.h>

struct Departure {
    String line;
    String direction;
    String mode;          // z. B. "bus", "tram", "subway", ...
    String plannedWhen;   // ISO-Zeitstring
    String when;          // tatsaechliche/prognostizierte ISO-Zeitstring
    int delayMinutes;     // Verspaetung in Minuten
    bool cancelled;
    long minutesUntil;    // Minuten bis zur Abfahrt (berechnet)
};
