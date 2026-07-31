#pragma once

#include <login.hpp>

// Bring WiFi up with a bounded timeout so a missing/booting AP can't hang the
// whole device forever (the old setup() spun in an unbounded while loop, so
// with WiFi down nothing - homing, web UI, MQTT - ever started).
void wifiSetup(unsigned long timeoutMs = 15000) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
        delay(250);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
    } else {
        Serial.println("\nWiFi connect timed out; continuing, will retry in background.");
    }
}

// Periodically nudge a dropped WiFi link back up. setAutoReconnect usually
// handles this, but this is a belt-and-braces recovery if it gives up.
void wifiLoop() {
    static unsigned long lastAttempt = 0;
    if (WiFi.status() != WL_CONNECTED && (millis() - lastAttempt > 10000)) {
        lastAttempt = millis();
        Serial.println("WiFi down, reconnecting...");
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
}
