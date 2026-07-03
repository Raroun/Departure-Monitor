#pragma once

#include <Arduino.h>
#include <vector>

#include "departure.h"

class DisplayManager {
   public:
    bool begin();

    // Shows the startup / error message.
    // Optionally a second detail line can be shown below the message.
    void showMessage(const char* title, const char* message, const char* detail = nullptr);

    // Shows a decorative night-mode screen.
    void showNightMode(const char* timeStr, const char* dateStr);

    // Draws the departure list for one stop.
    void showDepartures(const char* stopName, const std::vector<Departure>& departures);

    // Draws two lists (e.g. bus + train).
    // Optionally shows the last update time, battery info and error state in
    // the header area. If lastUpdateEpoch is provided, the timestamp is
    // highlighted when the data is older than 10 minutes.
    void showDepartures(const char* title1, const std::vector<Departure>& deps1, size_t max1,
                        const char* title2, const std::vector<Departure>& deps2, size_t max2,
                        const char* lastUpdate = nullptr,
                        time_t lastUpdateEpoch = 0);

    // Set battery state that will be shown on the next departure screen.
    // percent = -1 means unknown.
    void setBatteryInfo(int percent, bool low);

    // Set an optional error detail shown in the header and on error screens.
    void setErrorInfo(const char* error);

    // One-time full display refresh (removes ghosting).
    void fullRefresh();

    // Deep-sleep mode for the display (power saving).
    void sleep();

   private:
    void drawDepartureRow(int y, const Departure& dep, int index);
    String formatTime(long minutesUntil);

    void drawVehicleIcon(int x, int y, const String& mode);
    void drawBatteryIcon(int x, int y, int percent);
    void drawWarningIcon(int x, int y);
    void drawHeader(int y, const char* title, const char* lastUpdate = nullptr, time_t lastUpdateEpoch = 0);

    int batteryPercent_ = -1;
    bool batteryLow_ = false;
    const char* errorText_ = nullptr;
};
