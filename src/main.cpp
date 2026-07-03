#include <Arduino.h>
#include <algorithm>
#include <esp_sleep.h>
#include <esp_task_wdt.h>
#include <time.h>

#include "battery_manager.h"
#include "config.h"
#include "departure_api.h"
#include "display_manager.h"
#include "wifi_manager.h"

static const char* TAG = "Main";

// Diagnostic data in RTC memory: survives deep sleep, lost on power loss.
RTC_NOINIT_ATTR static uint32_t bootCount = 0;
RTC_NOINIT_ATTR static time_t lastSuccessUnix = 0;
RTC_NOINIT_ATTR static uint32_t lastErrorCode = 0;

enum class ErrorCode : uint32_t {
    NONE = 0,
    DISPLAY_INIT_FAILED = 1,
    WIFI_CONNECT_FAILED = 2,
    NTP_SYNC_FAILED = 3,
    API_FETCH_FAILED = 4,
    UNKNOWN = 99
};

WiFiManager wifi(WIFI_SSID, WIFI_PASSWORD);
DisplayManager display;
BatteryManager battery;

#if API_TYPE == API_VRR_EFA
DepartureApi api1(API_BASE_URL, STOP1_PLACE, STOP1_ID);
DepartureApi api2(API_BASE_URL, STOP2_PLACE, STOP2_ID);
#else
DepartureApi api1(API_BASE_URL, STOP1_ID);
DepartureApi api2(API_BASE_URL, STOP2_ID);
#endif

static bool lineMatches(const Departure& dep, const char** lines, size_t count) {
    if (count == 0) return true;
    for (size_t i = 0; i < count; ++i) {
        if (dep.line == lines[i]) return true;
    }
    return false;
}

static bool directionMatches(const Departure& dep, const char** directions, size_t count) {
    if (count == 0) return true;
    for (size_t i = 0; i < count; ++i) {
        if (dep.direction.indexOf(directions[i]) >= 0) return true;
    }
    return false;
}

static bool directionExcludes(const Departure& dep, const char** directions, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (dep.direction.indexOf(directions[i]) >= 0) return true;
    }
    return false;
}

static void filterDepartures(std::vector<Departure>& deps,
                              const char** allowedLines, size_t linesCount,
                              const char** allowedDirections, size_t directionsCount,
                              const char** excludedDirections, size_t excludedDirectionsCount) {
    deps.erase(std::remove_if(deps.begin(), deps.end(),
                              [&](const Departure& d) {
                                  return !lineMatches(d, allowedLines, linesCount) ||
                                         !directionMatches(d, allowedDirections, directionsCount) ||
                                         directionExcludes(d, excludedDirections, excludedDirectionsCount);
                              }),
               deps.end());
}

// Selects the next matching departure for each allowed line.
// This keeps the "columns" for S2 / RE3 / RB32 filled even when the next
// matching departure is further in the future.
static std::vector<Departure> selectNextPerLine(const std::vector<Departure>& deps,
                                                const char** lines, size_t linesCount) {
    std::vector<Departure> result;
    for (size_t i = 0; i < linesCount; ++i) {
        const Departure* best = nullptr;
        for (const auto& d : deps) {
            if (d.line == lines[i]) {
                if (best == nullptr || d.minutesUntil < best->minutesUntil) {
                    best = &d;
                }
            }
        }
        if (best != nullptr) {
            result.push_back(*best);
        }
    }
    std::sort(result.begin(), result.end(),
              [](const Departure& a, const Departure& b) {
                  return a.minutesUntil < b.minutesUntil;
              });
    return result;
}

static void applyGermanyTimezone() {
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
}

static void setError(ErrorCode code) {
    lastErrorCode = static_cast<uint32_t>(code);
}

static void goToSleep(unsigned long sleepSec) {
    wifi.disconnect();
    display.sleep();
    Serial.printf("[%s] Going to deep sleep for %lu seconds...\n", TAG, sleepSec);
    esp_sleep_enable_timer_wakeup(sleepSec * 1000000ULL);
    esp_deep_sleep_start();
}

static void logDiagnostics(esp_sleep_wakeup_cause_t wakeCause, bool coldBoot) {
    bootCount++;
    Serial.printf("[%s] Boot count: %lu\n", TAG, bootCount);
    Serial.printf("[%s] Cold boot: %s\n", TAG, coldBoot ? "yes" : "no");
    Serial.printf("[%s] Wake cause: %d\n", TAG, wakeCause);
    if (lastSuccessUnix > 1700000000) {
        struct tm tm;
        localtime_r(&lastSuccessUnix, &tm);
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
        Serial.printf("[%s] Last successful update: %s\n", TAG, buf);
    } else {
        Serial.printf("[%s] Last successful update: never\n", TAG);
    }
    Serial.printf("[%s] Last error code: %lu\n", TAG, lastErrorCode);
}

void setupTime() {
    // configTime() resets the TZ environment variable to UTC, so we apply the
    // Germany timezone before AND after it to stay in local time.
    applyGermanyTimezone();
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    applyGermanyTimezone();

    Serial.printf("[%s] Waiting for NTP time...\n", TAG);
    time_t now = time(nullptr);
    int retries = 0;
    while (now < 1700000000 && retries < 30) {
        delay(500);
        esp_task_wdt_reset();
        now = time(nullptr);
        retries++;
        Serial.print(".");
    }
    Serial.println();

    applyGermanyTimezone();
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    Serial.printf("[%s] Current time: %04d-%02d-%02d %02d:%02d:%02d\n",
                  TAG, timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    if (now < 1700000000) {
        setError(ErrorCode::NTP_SYNC_FAILED);
        display.setErrorInfo("NTP");
        display.showMessage("Warning", "NTP sync failed.", "Using RTC time if available.");
        Serial.printf("[%s] NTP sync failed, using RTC time if available\n", TAG);
    }
}

static bool isNightHour(int hour) {
    return (hour >= NIGHT_START_HOUR) || (hour < NIGHT_END_HOUR);
}

unsigned long getSleepInterval() {
    applyGermanyTimezone();
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    int hour = timeinfo.tm_hour;
    bool isNight = isNightHour(hour);
    unsigned long interval = isNight ? NIGHT_INTERVAL_SEC : DAY_INTERVAL_SEC;

    // Low battery: poll less often during the day to extend runtime.
    if (!isNight && battery.isLow()) {
        interval = LOW_BATTERY_INTERVAL_SEC;
        Serial.printf("[%s] Battery low (%d%%), extending interval\n", TAG, battery.lastPercent());
    }

    Serial.printf("[%s] Time of day: %02d:%02d -> %s, interval %lu sec\n",
                  TAG, timeinfo.tm_hour, timeinfo.tm_min,
                  isNight ? "night" : "day", interval);
    return interval;
}

static unsigned long secondsUntilMorning(const struct tm& timeinfo) {
    int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    int morningMinutes = NIGHT_END_HOUR * 60;  // 05:00
    int diffMinutes = morningMinutes - currentMinutes;
    if (diffMinutes <= 0) {
        diffMinutes += 24 * 60;  // next morning (should not happen in night mode)
    }
    if (diffMinutes < 1) diffMinutes = 1;
    return diffMinutes * 60UL;
}

static void enterNightMode(const struct tm& timeinfo) {
    unsigned long sleepSec = secondsUntilMorning(timeinfo);

    // The internal ESP32 RTC drifts slightly over several hours.
    // To wake up reliably around 5:00, we sleep at most 2 hours at a time
    // and then re-check the time.
    const unsigned long MAX_NIGHT_SLEEP_SEC = 7200;  // 2 hours
    if (sleepSec > MAX_NIGHT_SLEEP_SEC) {
        sleepSec = MAX_NIGHT_SLEEP_SEC;
    }

    char timeStr[16];
    char dateStr[32];
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
    strftime(dateStr, sizeof(dateStr), "%d.%m.%Y", &timeinfo);

    display.showNightMode(timeStr, dateStr);
    Serial.printf("[%s] Night mode, sleeping %lu sec (max. until 05:00)...\n", TAG, sleepSec);
    goToSleep(sleepSec);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Time zone for Germany with automatic summer/winter time.
    // Must be set first, before time()/localtime_r() is called, because the
    // RAM (and therefore the TZ variable) is re-initialized after reset/deep sleep.
    applyGermanyTimezone();

    // Turn off Bluetooth completely to save power in deep sleep
    btStop();

    battery.begin();

    // Distinguish cold boot vs. waking up from deep sleep.
    // After deep sleep we don't need intermediate screens – saves time, power
    // and avoids flickering.
    bool coldBoot = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED);

    Serial.printf("\n[%s] Departure Monitor starting...\n", TAG);
    logDiagnostics(esp_sleep_get_wakeup_cause(), coldBoot);

    if (!display.begin()) {
        setError(ErrorCode::DISPLAY_INIT_FAILED);
        Serial.printf("[%s] Display initialization failed!\n", TAG);
        // Sleep and retry instead of blocking forever.
        esp_sleep_enable_timer_wakeup(300 * 1000000ULL);
        esp_deep_sleep_start();
    }

    int batteryPercent = battery.readPercent();
    display.setBatteryInfo(batteryPercent, battery.isLow());
    Serial.printf("[%s] Battery: %d%%\n", TAG, batteryPercent);

    // If the time is already known from RTC and it is night, skip Wi-Fi,
    // NTP and API requests completely.
    applyGermanyTimezone();
    time_t now = time(nullptr);
    bool timeKnown = (now > 1700000000);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    if (timeKnown && isNightHour(timeinfo.tm_hour)) {
        enterNightMode(timeinfo);
    }

    if (coldBoot) {
        display.showMessage("Departure Monitor V1.0 by Raroun", "Connecting to WiFi...");
    }

    if (!wifi.connect()) {
        setError(ErrorCode::WIFI_CONNECT_FAILED);
        display.setErrorInfo("WiFi");
        display.showMessage("Error", "WiFi connection failed.", "Retrying in 3 minutes.");
        Serial.printf("[%s] WiFi failed, going to deep sleep...\n", TAG);
        goToSleep(DAY_INTERVAL_SEC);
    }

    setupTime();

    // After NTP sync it can still be night (e.g. reset at 21:30).
    now = time(nullptr);
    applyGermanyTimezone();
    localtime_r(&now, &timeinfo);
    if (isNightHour(timeinfo.tm_hour)) {
        enterNightMode(timeinfo);
    }

    if (coldBoot) {
        display.showMessage("Departure Monitor V1.0 by Raroun", "Loading departures...");
    }

    std::vector<Departure> deps1, deps2;
    bool ok1 = api1.fetch(deps1);
    bool ok2 = api2.fetch(deps2);

    if (!ok1 || !ok2) {
        setError(ErrorCode::API_FETCH_FAILED);
        display.setErrorInfo("API");
        Serial.printf("[%s] API fetch failed (stop1=%s, stop2=%s)\n", TAG, ok1 ? "ok" : "fail", ok2 ? "ok" : "fail");
    }

    Serial.printf("[%s] Raw data: Wesselstr=%u, Hbf=%u\n", TAG, deps1.size(), deps2.size());

    if (ok1) {
        filterDepartures(deps1, STOP1_LINES, STOP1_LINES_COUNT,
                         nullptr, 0,
                         STOP1_EXCLUDE_DIRECTIONS, STOP1_EXCLUDE_DIRECTIONS_COUNT);
    }
    if (ok2) {
        filterDepartures(deps2, STOP2_LINES, STOP2_LINES_COUNT,
                         STOP2_DIRECTIONS, STOP2_DIRECTIONS_COUNT,
                         nullptr, 0);
    }

    // For the main station: select the next matching departure per line
    // (S2/RE3/RB32) so the three "columns" stay filled further into the future.
    std::vector<Departure> deps2PerLine = selectNextPerLine(deps2, STOP2_LINES, STOP2_LINES_COUNT);

    Serial.printf("[%s] Filtered: Wesselstr=%u, Hbf=%u\n", TAG, deps1.size(), deps2.size());
    for (const auto& d : deps2PerLine) {
        Serial.printf("[%s] Hbf: line=%s direction=%s minutes=%ld\n",
                      TAG, d.line.c_str(), d.direction.c_str(), d.minutesUntil);
    }

    if (!deps1.empty() || !deps2PerLine.empty()) {
        if (coldBoot) {
            display.fullRefresh();
        }
        // Format current time as last update time
        now = time(nullptr);
        applyGermanyTimezone();
        localtime_r(&now, &timeinfo);
        char lastUpdateStr[16];
        strftime(lastUpdateStr, sizeof(lastUpdateStr), "%H:%M", &timeinfo);
        display.showDepartures(STOP1_TITLE, deps1, 4, STOP2_TITLE, deps2PerLine, 3, lastUpdateStr, now);
        lastSuccessUnix = time(nullptr);
        setError(ErrorCode::NONE);
    } else {
        setError(ErrorCode::API_FETCH_FAILED);
        if (!ok1 && !ok2) {
            display.showMessage("Error", "API request failed.", "Check stop IDs and API availability.");
        } else {
            display.showMessage("Info", "No matching departures found.", "Check filters or try again later.");
        }
    }

    unsigned long sleepSec = getSleepInterval();
    goToSleep(sleepSec);
}

void loop() {
    // Never reached because deep sleep is used in setup()
}
