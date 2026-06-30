#include <Arduino.h>
#include <algorithm>
#include <esp_sleep.h>
#include <time.h>

#include "config.h"
#include "departure_api.h"
#include "display_manager.h"
#include "wifi_manager.h"

static const char* TAG = "Main";

WiFiManager wifi(WIFI_SSID, WIFI_PASSWORD);
DisplayManager display;

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

// Waehlt fuer jede erlaubte Linie die naechste passende Abfahrt aus.
// Damit bleiben die "Spalten" fuer S2 / RE3 / RB32 auch dann gefuellt,
// wenn die naechste passende Fahrt erst weiter in der Zukunft liegt.
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

void setupTime() {
    // Zeitzone Deutschland mit automatischer Sommer-/Winterzeit
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
    Serial.printf("[%s] Warte auf NTP-Zeit...\n", TAG);
    time_t now = time(nullptr);
    int retries = 0;
    while (now < 1700000000 && retries < 30) {
        delay(500);
        now = time(nullptr);
        retries++;
        Serial.print(".");
    }
    Serial.println();
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    Serial.printf("[%s] Aktuelle Zeit: %04d-%02d-%02d %02d:%02d:%02d\n",
                  TAG, timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

static bool isNightHour(int hour) {
    return (hour >= NIGHT_START_HOUR) || (hour < NIGHT_END_HOUR);
}

unsigned long getSleepInterval() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    int hour = timeinfo.tm_hour;
    bool isNight = isNightHour(hour);
    unsigned long interval = isNight ? NIGHT_INTERVAL_SEC : DAY_INTERVAL_SEC;
    Serial.printf("[%s] Tageszeit: %02d:%02d -> %s, Intervall %lu Sek.\n",
                  TAG, timeinfo.tm_hour, timeinfo.tm_min,
                  isNight ? "Nacht" : "Tag", interval);
    return interval;
}

static unsigned long secondsUntilMorning(const struct tm& timeinfo) {
    int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    int morningMinutes = NIGHT_END_HOUR * 60;  // 05:00
    int diffMinutes = morningMinutes - currentMinutes;
    if (diffMinutes <= 0) {
        diffMinutes += 24 * 60;  // naechster Morgen (sollte im Nachtmodus nicht vorkommen)
    }
    if (diffMinutes < 1) diffMinutes = 1;
    return diffMinutes * 60UL;
}

static void enterNightMode(const struct tm& timeinfo) {
    unsigned long sleepSec = secondsUntilMorning(timeinfo);

    // Der interne RTC des ESP32 driftet ueber mehrere Stunden leicht.
    // Um verlaesslich um 5:00 Uhr aufzuwachen, schlafen wir maximal 2 Stunden
    // am Stueck und pruefen dann die Zeit neu.
    const unsigned long MAX_NIGHT_SLEEP_SEC = 7200;  // 2 Stunden
    if (sleepSec > MAX_NIGHT_SLEEP_SEC) {
        sleepSec = MAX_NIGHT_SLEEP_SEC;
    }

    char timeStr[16];
    char dateStr[32];
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
    strftime(dateStr, sizeof(dateStr), "%d.%m.%Y", &timeinfo);

    display.showNightMode(timeStr, dateStr);
    display.sleep();
    Serial.printf("[%s] Nachtmodus, schlafe %lu Sek. (max. bis 05:00)...\n", TAG, sleepSec);
    esp_sleep_enable_timer_wakeup(sleepSec * 1000000ULL);
    esp_deep_sleep_start();
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Bluetooth komplett abschalten, spart Strom im Deep Sleep
    btStop();

    Serial.printf("\n[%s] Abfahrtsmonitor startet...\n", TAG);

    // Unterscheidung Erststart (cold boot) vs. Aufwachen aus Deep Sleep.
    // Nach Deep Sleep brauchen wir keine Zwischenscreens mehr – das spart
    // Zeit, Strom und vermeidet Flackern.
    bool coldBoot = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED);
    Serial.printf("[%s] Cold boot: %s\n", TAG, coldBoot ? "ja" : "nein");

    if (!display.begin()) {
        Serial.printf("[%s] Display-Initialisierung fehlgeschlagen!\n", TAG);
        while (true) delay(1000);
    }

    // Wenn die Zeit aus dem RTC bereits bekannt ist und es Nacht ist,
    // sparen wir uns WLAN, NTP und API-Abfragen komplett.
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
        display.showMessage("Error", "WiFi connection failed.\nPlease check configuration.");
        Serial.printf("[%s] WLAN fehlgeschlagen, gehe in Deepsleep...\n", TAG);
        esp_sleep_enable_timer_wakeup(DAY_INTERVAL_SEC * 1000000ULL);
        esp_deep_sleep_start();
    }

    setupTime();

    // Auch nach dem NTP-Sync kann es jetzt Nacht sein (z. B. Reset um 21:30).
    now = time(nullptr);
    localtime_r(&now, &timeinfo);
    if (isNightHour(timeinfo.tm_hour)) {
        wifi.disconnect();
        enterNightMode(timeinfo);
    }

    if (coldBoot) {
        display.showMessage("Departure Monitor V1.0 by Raroun", "Loading departures...");
    }

    std::vector<Departure> deps1, deps2;
    bool ok1 = api1.fetch(deps1);
    bool ok2 = api2.fetch(deps2);

    Serial.printf("[%s] Rohdaten: Wesselstr=%u, Hbf=%u\n", TAG, deps1.size(), deps2.size());

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

    // Fuer den Hbf: Pro Linie (S2/RE3/RB32) die naechste passende Abfahrt
    // auswaehlen, damit die drei "Spalten" auch weiter in der Zukunft gefuellt sind.
    std::vector<Departure> deps2PerLine = selectNextPerLine(deps2, STOP2_LINES, STOP2_LINES_COUNT);

    Serial.printf("[%s] Gefiltert: Wesselstr=%u, Hbf=%u\n", TAG, deps1.size(), deps2.size());
    for (const auto& d : deps2PerLine) {
        Serial.printf("[%s] Hbf: line=%s direction=%s minutes=%ld\n",
                      TAG, d.line.c_str(), d.direction.c_str(), d.minutesUntil);
    }

    if (!deps1.empty() || !deps2PerLine.empty()) {
        if (coldBoot) {
            display.fullRefresh();
        }
        // Aktuelle Uhrzeit als letzte Aktualisierungszeit formatieren
        now = time(nullptr);
        localtime_r(&now, &timeinfo);
        char lastUpdateStr[16];
        strftime(lastUpdateStr, sizeof(lastUpdateStr), "%H:%M", &timeinfo);
        display.showDepartures(STOP1_TITLE, deps1, 4, STOP2_TITLE, deps2PerLine, 3, lastUpdateStr);
    } else {
        display.showMessage("Error", "No matching departures found.");
    }

    unsigned long sleepSec = getSleepInterval();
    wifi.disconnect();
    display.sleep();

    Serial.printf("[%s] Gehe in Deepsleep fuer %lu Sekunden...\n", TAG, sleepSec);
    esp_sleep_enable_timer_wakeup(sleepSec * 1000000ULL);
    esp_deep_sleep_start();
}

void loop() {
    // Wird nie erreicht, da Deepsleep im setup() verwendet wird
}
