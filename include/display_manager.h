#pragma once

#include <Arduino.h>
#include <vector>

#include "departure.h"

class DisplayManager {
   public:
    bool begin();

    // Shows the startup / error message
    void showMessage(const char* title, const char* message);

    // Shows a decorative night-mode screen
    void showNightMode(const char* timeStr, const char* dateStr);

    // Draws the departure list for one stop
    void showDepartures(const char* stopName, const std::vector<Departure>& departures);

    // Draws two lists (e.g. bus + train).
    // Optionally shows the last update time in the top-right corner.
    void showDepartures(const char* title1, const std::vector<Departure>& deps1, size_t max1,
                        const char* title2, const std::vector<Departure>& deps2, size_t max2,
                        const char* lastUpdate = nullptr);

    // One-time full display refresh (removes ghosting)
    void fullRefresh();

    // Deep-sleep mode for the display (power saving)
    void sleep();

   private:
    void drawDepartureRow(int y, const Departure& dep, int index);
    String formatTime(long minutesUntil);
};
