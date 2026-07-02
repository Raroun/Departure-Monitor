#include "departure_api.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <esp_task_wdt.h>
#include <time.h>

static const char* TAG = "DepartureAPI";

DepartureApi::DepartureApi(const char* baseUrl, const char* stopId)
    : baseUrl_(baseUrl), stopPlace_(""), stopId_(stopId) {}

DepartureApi::DepartureApi(const char* baseUrl, const char* stopPlace, const char* stopId)
    : baseUrl_(baseUrl), stopPlace_(stopPlace), stopId_(stopId) {}

String DepartureApi::buildUrl() {
#if API_TYPE == API_VRR_EFA
    String url = baseUrl_;
    url += "/XML_DM_REQUEST?outputFormat=rapidJSON&version=10.4.18.18";
    url += "&place_dm=";
    url += stopPlace_;
    url += "&placeState_dm=empty";
    url += "&type_dm=stop";
    url += "&name_dm=";
    url += stopId_;
    url += "&mode=direct";
    url += "&useRealtime=1";
    url += "&limit=50";  // Far enough into the future so all lines are filled
    return url;
#else
    String url = baseUrl_;
    url += "/stops/";
    url += stopId_;
    url += "/departures?duration=60&results=";
    url += String(MAX_DEPARTURES + 4);
    return url;
#endif
}

// Parses "YYYY-MM-DDTHH:MM:SS+02:00" or "YYYY-MM-DDTHH:MM:SSZ" up to the
// seconds. The rest (offset / Z) is ignored.
static bool parseIsoTime(const char* str, struct tm* outTm) {
    if (str == nullptr || strlen(str) < 19) return false;
    int y, m, d, h, min, s;
    int n = sscanf(str, "%d-%d-%dT%d:%d:%d", &y, &m, &d, &h, &min, &s);
    if (n != 6) return false;

    memset(outTm, 0, sizeof(struct tm));
    outTm->tm_year = y - 1900;
    outTm->tm_mon = m - 1;
    outTm->tm_mday = d;
    outTm->tm_hour = h;
    outTm->tm_min = min;
    outTm->tm_sec = s;
    outTm->tm_isdst = 0;  // ISO time strings contain the offset, so no DST
    return true;
}

#if API_TYPE == API_VRR_EFA
// Custom UTC mktime implementation because timegm() is not available on ESP32.
static time_t utc_mktime(struct tm* tm) {
    static const int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int year = tm->tm_year + 1900;
    int month = tm->tm_mon;
    int day = tm->tm_mday - 1;  // zero-based

    long days = (year - 1970) * 365;
    for (int y = 1970; y < year; y++) {
        if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) days++;
    }
    for (int m = 0; m < month; m++) {
        days += days_in_month[m];
        if (m == 1 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) days++;
    }
    days += day;

    return (time_t)(days * 86400LL + tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec);
}

static time_t parseUtcIsoTime(const char* str) {
    struct tm tm;
    if (!parseIsoTime(str, &tm)) return 0;
    return utc_mktime(&tm);
}

static bool parseVrrDepartures(const JsonDocument& doc, std::vector<Departure>& out) {
    out.clear();
    time_t now = time(nullptr);

    JsonArrayConst stopEvents = doc["stopEvents"];
    if (stopEvents.isNull()) {
        Serial.printf("[%s] No stopEvents in JSON\n", TAG);
        return false;
    }

    for (JsonObjectConst event : stopEvents) {
        Departure d;

        JsonObjectConst transportation = event["transportation"];
        d.line = transportation["disassembledName"] | transportation["number"] | "?";
        d.direction = transportation["destination"]["name"] | "?";
        d.mode = transportation["product"]["name"] | "bus";

        // VRR EFA returns times as UTC ISO strings (ending in 'Z')
        d.when = event["departureTimeEstimated"] | event["departureTimePlanned"] | "";
        d.plannedWhen = event["departureTimePlanned"] | "";
        d.cancelled = event["isCancelled"] | false;

        time_t tWhen = parseUtcIsoTime(d.when.c_str());
        time_t tPlanned = parseUtcIsoTime(d.plannedWhen.c_str());
        d.delayMinutes = (int)difftime(tWhen, tPlanned) / 60;
        d.minutesUntil = (long)difftime(tWhen, now) / 60;

        Serial.printf("[%s] VRR raw: line=%s direction=%s when=%s planned=%s -> minutes=%ld delay=%d\n",
                      TAG, d.line.c_str(), d.direction.c_str(), d.when.c_str(), d.plannedWhen.c_str(),
                      d.minutesUntil, d.delayMinutes);

        out.push_back(d);
        if (out.size() >= MAX_DEPARTURES) break;
    }

    return !out.empty();
}
#else
static bool parseTransportRestDepartures(const JsonDocument& doc, std::vector<Departure>& out) {
    out.clear();
    time_t now = time(nullptr);

    JsonArrayConst departures = doc;
    if (departures.isNull()) {
        Serial.printf("[%s] No departures in JSON\n", TAG);
        return false;
    }

    for (JsonObjectConst dep : departures) {
        Departure d;
        d.line = dep["line"]["name"] | "?";
        d.direction = dep["direction"] | "?";
        d.mode = dep["line"]["mode"] | "bus";
        d.when = dep["when"] | "";
        d.plannedWhen = dep["plannedWhen"] | "";
        d.cancelled = dep["cancelled"] | false;

        d.delayMinutes = 0;
        struct tm tmWhen = {};
        struct tm tmPlanned = {};
        if (parseIsoTime(d.when.c_str(), &tmWhen) && parseIsoTime(d.plannedWhen.c_str(), &tmPlanned)) {
            time_t tWhen = mktime(&tmWhen);
            time_t tPlanned = mktime(&tmPlanned);
            d.delayMinutes = (int)difftime(tWhen, tPlanned) / 60;
        }

        d.minutesUntil = 0;
        if (parseIsoTime(d.when.c_str(), &tmWhen)) {
            time_t tWhen = mktime(&tmWhen);
            d.minutesUntil = (long)difftime(tWhen, now) / 60;
        }

        out.push_back(d);
        if (out.size() >= MAX_DEPARTURES) break;
    }

    return !out.empty();
}
#endif

bool DepartureApi::fetch(std::vector<Departure>& out) {
    HTTPClient http;
    String url = buildUrl();
    Serial.printf("[%s] GET %s\n", TAG, url.c_str());

    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setConnectTimeout(10000);  // TLS handshake can be slow
    http.setTimeout(8000);          // Must stay below the task watchdog timeout
    http.begin(url);

    esp_task_wdt_reset();
    int httpCode = http.GET();
    esp_task_wdt_reset();

    if (httpCode != 200) {
        Serial.printf("[%s] HTTP error: %d\n", TAG, httpCode);
        http.end();
        return false;
    }

    int contentLength = http.getSize();
    Serial.printf("[%s] HTTP content-length: %d\n", TAG, contentLength);

    JsonDocument doc;
    DeserializationError err;

#if API_TYPE == API_VRR_EFA
    // Save memory: only read the fields we need from the large rapidJSON
    // response. The VRR server delivers up to 40 stopEvents; reading the whole
    // response with http.getString() would overload the ESP32 RAM.
    JsonDocument filter;
    filter["stopEvents"][0]["departureTimePlanned"] = true;
    filter["stopEvents"][0]["departureTimeEstimated"] = true;
    filter["stopEvents"][0]["isCancelled"] = true;
    filter["stopEvents"][0]["transportation"]["disassembledName"] = true;
    filter["stopEvents"][0]["transportation"]["number"] = true;
    filter["stopEvents"][0]["transportation"]["destination"]["name"] = true;
    filter["stopEvents"][0]["transportation"]["product"]["name"] = true;

    err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
#else
    err = deserializeJson(doc, http.getStream());
#endif

    http.end();

    if (err) {
        Serial.printf("[%s] JSON parse error: %s\n", TAG, err.c_str());
        return false;
    }

#if API_TYPE == API_VRR_EFA
    bool ok = parseVrrDepartures(doc, out);
#else
    bool ok = parseTransportRestDepartures(doc, out);
#endif

    Serial.printf("[%s] %u departures loaded\n", TAG, out.size());
    return ok;
}
