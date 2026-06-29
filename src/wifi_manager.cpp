#include "wifi_manager.h"

#include <WiFi.h>

static const char* TAG = "WiFiManager";

WiFiManager::WiFiManager(const char* ssid, const char* password)
    : ssid_(ssid), password_(password) {}

bool WiFiManager::connect(unsigned long timeoutMs) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid_, password_);

    Serial.printf("[%s] Verbinde mit %s", TAG, ssid_);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[%s] Verbunden, IP: %s\n", TAG, WiFi.localIP().toString().c_str());
        return true;
    }

    Serial.printf("[%s] Verbindung fehlgeschlagen!\n", TAG);
    return false;
}

bool WiFiManager::isConnected() const { return WiFi.status() == WL_CONNECTED; }

void WiFiManager::disconnect() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

String WiFiManager::localIP() const { return WiFi.localIP().toString(); }
