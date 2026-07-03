#pragma once

#include <Arduino.h>

class BatteryManager {
   public:
    BatteryManager();

    // Initialize the ADC pin for battery voltage reading.
    void begin();

    // Read the current battery level in percent (0-100).
    // Returns -1 if the reading is invalid.
    int readPercent();

    // Returns true if the battery is considered low (< 20%).
    bool isLow() const { return lastPercent_ >= 0 && lastPercent_ < 20; }

    // Returns the last measured percentage.
    int lastPercent() const { return lastPercent_; }

   private:
    int lastPercent_;
};
