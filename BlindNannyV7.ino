#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <TMCStepper.h>
#include <HardwareSerial.h>
#include <AccelStepper.h>
#include <PubSubClient.h> 
#include <Preferences.h>
#include <time.h>
#include <math.h>

#include <login.hpp>
#include <config.hpp>
#include <network.hpp>
#include <blinds.hpp>
#include <mqtt.hpp>
#include <web.hpp>
#include <ota.hpp>

void setup() {
    Serial.begin(115200);
    Serial.println("--- Starting BlindNanny ---");

    configSetup();

    wifiSetup();

    blindSetup();
    mqttSetup();
    webServerSetup();
    otaSetup();
}

void loop() {
    wifiLoop();
    otaLoop();
    mqttLoop();
    blindLoop();
    solarLoop();

    // A structural config change (e.g. motor count) asked for a reboot; give
    // the HTTP response a moment to flush, then restart.
    if (rebootRequested) { delay(300); ESP.restart(); }
}