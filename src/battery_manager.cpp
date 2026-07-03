#include "battery_manager.h"

#include "utilities.h"  // BATT_PIN from LilyGo-EPD47

// The T5 4.7" S3 uses a voltage divider so the ADC pin sees half of the
// battery voltage. A full LiPo cell is ~4.2 V, empty is ~3.0 V.
static constexpr float VOLTAGE_DIVIDER_RATIO = 2.0f;
static constexpr float ADC_REF_VOLTAGE = 3.3f;
static constexpr int ADC_RESOLUTION = 4095;  // 12-bit
static constexpr float BATTERY_FULL = 4.2f;
static constexpr float BATTERY_EMPTY = 3.0f;

BatteryManager::BatteryManager() : lastPercent_(-1) {}

void BatteryManager::begin() {
    pinMode(BATT_PIN, INPUT);
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
}

int BatteryManager::readPercent() {
    int raw = analogRead(BATT_PIN);
    if (raw <= 0 || raw >= ADC_RESOLUTION) {
        lastPercent_ = -1;
        return -1;
    }

    float voltage = (raw / (float)ADC_RESOLUTION) * ADC_REF_VOLTAGE * VOLTAGE_DIVIDER_RATIO;
    int percent = (int)((voltage - BATTERY_EMPTY) / (BATTERY_FULL - BATTERY_EMPTY) * 100.0f);
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    lastPercent_ = percent;
    return percent;
}
