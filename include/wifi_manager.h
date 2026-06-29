#pragma once

#include <Arduino.h>
#include <WiFi.h>

class WiFiManager {
   public:
    WiFiManager(const char* ssid, const char* password);

    bool connect(unsigned long timeoutMs = 20000);
    bool isConnected() const;
    void disconnect();
    String localIP() const;

   private:
    const char* ssid_;
    const char* password_;
};
