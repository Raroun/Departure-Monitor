#pragma once

#include <Arduino.h>
#include <vector>

#include "departure.h"

class DisplayManager {
   public:
    bool begin();

    // Zeigt die Start-/Fehlermeldung
    void showMessage(const char* title, const char* message);

    // Zeichnet die Liste der Abfahrten (eine Haltestelle)
    void showDepartures(const char* stopName, const std::vector<Departure>& departures);

    // Zeichnet zwei Listen (z. B. Bus + Bahn)
    void showDepartures(const char* title1, const std::vector<Departure>& deps1, size_t max1,
                        const char* title2, const std::vector<Departure>& deps2, size_t max2);

    // Einmaliger kompletter Display-Refresh (löscht Geisterbilder)
    void fullRefresh();

    // Tiefenschlaf-Modus fuer das Display (stromsparend)
    void sleep();

   private:
    void drawDepartureRow(int y, const Departure& dep, int index);
    String formatTime(long minutesUntil);
};
