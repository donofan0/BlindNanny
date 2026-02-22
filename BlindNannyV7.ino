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
#include <blinds.hpp>
#include <mqtt.hpp>
#include <web.hpp>

void setup() {
    Serial.begin(115200);
    Serial.println("--- Starting BlindNanny ---");

    configSetup();

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) { 
        delay(500); 
        Serial.print("."); 
    }
    Serial.println(WiFi.localIP());

    blindSetup();
    mqttSetup();
    webServerSetup();
}

void loop() {
    mqttLoop();
    blindLoop();
    solarLoop();
}