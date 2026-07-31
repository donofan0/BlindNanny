#pragma once

#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <login.hpp>

// Over-the-air firmware updates + mDNS, so the device can be reflashed and
// reached by name (http://<hostName>.local) without a USB cable.
//
// Push a build with the Arduino IDE / arduino-cli espota over the network,
// or PlatformIO's espota upload. Note: the blocking homing loop (see
// blinds.hpp) starves ArduinoOTA.handle() while a calibrate runs, so don't
// push an update during calibration - retry once it finishes.
void otaSetup() {
    ArduinoOTA.setHostname(hostName.c_str());
#ifdef OTA_PASSWORD
    ArduinoOTA.setPassword(OTA_PASSWORD);
#endif
    ArduinoOTA.onStart([]() { Serial.println("OTA update starting..."); });
    ArduinoOTA.onEnd([]()   { Serial.println("OTA update complete."); });
    ArduinoOTA.onError([](ota_error_t e) { Serial.printf("OTA error [%u]\n", e); });
    ArduinoOTA.begin();

    if (MDNS.begin(hostName.c_str())) {
        MDNS.addService("http", "tcp", 80);   // enables http://<hostName>.local
        Serial.println("Reachable at http://" + hostName + ".local");
    }
}

void otaLoop() {
    ArduinoOTA.handle();
}
