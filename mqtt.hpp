#pragma once

#include <login.hpp>

String availTopic;   // MQTT Last-Will / availability topic (set in mqttSetup)

// Shared "device" block so every entity groups under one HA device card,
// with real name/manufacturer/model/version metadata.
String devBlock() {
    return "\"dev\":{\"ids\":[\"" + deviceId + "\"],"
           "\"name\":\"BlindNanny\",\"mf\":\"BlindNanny\","
           "\"mdl\":\"ESP32 Smart Blind\",\"sw\":\"7.3\"}";
}

// Shared availability block -> HA shows the device offline when the ESP drops.
String availBlock() {
    return "\"avty_t\":\"" + availTopic + "\","
           "\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\"";
}

// Announce device to Home Assistant for auto-discovery
void publishDiscovery() {
    bool multi = cfg_motor_count > 1;

    // --- Entities present in every mode ---
    // Calibrate button
    String plCal = "{\"name\":\"Calibrate\","
        "\"unique_id\":\"" + deviceId + "_cal\","
        "\"cmd_t\":\"" + baseTopic + "/calibrate\","
        "\"icon\":\"mdi:arrow-collapse-down\"," + availBlock() + "," + devBlock() + "}";
    client.publish(("homeassistant/button/" + deviceId + "_cal/config").c_str(), plCal.c_str(), true);

    // IP Address sensor
    String plIP = "{\"name\":\"IP Address\","
        "\"unique_id\":\"" + deviceId + "_ip\","
        "\"stat_t\":\"" + baseTopic + "/ip_address\","
        "\"icon\":\"mdi:ip-network\"," + availBlock() + "," + devBlock() + "}";
    client.publish(("homeassistant/sensor/" + deviceId + "_ip/config").c_str(), plIP.c_str(), true);

    // Auto sun-block switch
    String plAuto = "{\"name\":\"Auto Sun-Block\","
        "\"unique_id\":\"" + deviceId + "_auto\","
        "\"cmd_t\":\"" + baseTopic + "/set_auto\","
        "\"stat_t\":\"" + baseTopic + "/auto_state\","
        "\"pl_on\":\"ON\",\"pl_off\":\"OFF\","
        "\"icon\":\"mdi:sun-clock\"," + availBlock() + "," + devBlock() + "}";
    client.publish(("homeassistant/switch/" + deviceId + "_auto/config").c_str(), plAuto.c_str(), true);

    if (!multi) {
        // Single blind: one combined cover. (No separate percent-closed number
        // - it just duplicated the cover's own position control.)
        String pl = "{\"name\":null,"
            "\"unique_id\":\"" + deviceId + "\","
            "\"cmd_t\":\"" + baseTopic + "/command\","
            "\"pos_t\":\"" + baseTopic + "/position\","
            "\"set_pos_t\":\"" + baseTopic + "/set_position\","
            "\"stat_t\":\"" + baseTopic + "/state\","
            "\"pl_open\":\"OPEN\",\"pl_cls\":\"CLOSE\",\"pl_stop\":\"STOP\","
            "\"pos_open\":100,\"pos_clsd\":0,"
            "\"dev_cla\":\"blind\"," + availBlock() + "," + devBlock() + "}";
        client.publish(("homeassistant/cover/" + deviceId + "/config").c_str(), pl.c_str(), true);

        // Remove any Left/Right covers left over from a previous 2-motor config.
        client.publish(("homeassistant/cover/" + deviceId + "_left/config").c_str(), "", true);
        client.publish(("homeassistant/cover/" + deviceId + "_right/config").c_str(), "", true);
    } else {
        // Two blinds: individual Left/Right covers, no misleading combined
        // cover that only reflected motor 1.
        const char* sides[2]  = {"left", "right"};
        const char* names[2]  = {"Left Blind", "Right Blind"};
        for (int i = 0; i < 2; i++) {
            String side = sides[i];
            String pSide = "{\"name\":\"" + String(names[i]) + "\","
                "\"unique_id\":\"" + deviceId + "_" + side + "\","
                "\"cmd_t\":\"" + baseTopic + "/" + side + "/command\","
                "\"pos_t\":\"" + baseTopic + "/" + side + "/position\","
                "\"set_pos_t\":\"" + baseTopic + "/" + side + "/set_position\","
                "\"stat_t\":\"" + baseTopic + "/" + side + "/state\","
                "\"pl_open\":\"OPEN\",\"pl_cls\":\"CLOSE\",\"pl_stop\":\"STOP\","
                "\"pos_open\":100,\"pos_clsd\":0,\"dev_cla\":\"blind\","
                + availBlock() + "," + devBlock() + "}";
            client.publish(("homeassistant/cover/" + deviceId + "_" + side + "/config").c_str(),
                           pSide.c_str(), true);
        }
        // Remove the combined cover / number left over from a 1-motor config.
        client.publish(("homeassistant/cover/" + deviceId + "/config").c_str(), "", true);
        client.publish(("homeassistant/number/" + deviceId + "_pct/config").c_str(), "", true);
    }
}

// Derive a cover state string ("opening"/"closing"/"open"/"closed"/"stopped")
// from a stepper and its percent-closed value.
String coverState(AccelStepper &s, int pct) {
    if (s.isRunning()) return (s.distanceToGo() > 0) ? "closing" : "opening";
    if (pct >= 99) return "closed";
    if (pct <= 1)  return "open";
    return "stopped";
}

// Publish MQTT State
void publishState() {
    if (!client.connected()) return;

    client.publish((baseTopic + "/ip_address").c_str(), WiFi.localIP().toString().c_str());

    if (cfg_motor_count > 1) {
        // Two blinds: report each side; no combined/aggregate topic (it only
        // ever reflected motor 1 and was misleading when the sides differed).
        AccelStepper* steppers[2] = {&stepper1, &stepper2};
        const char* sides[2] = {"left", "right"};
        for (int i = 0; i < 2; i++) {
            long mp = getMaxPosition(i + 1);
            int sp = constrain((int)map(steppers[i]->currentPosition(), -mp, 0, 0, 100), 0, 100);
            client.publish((baseTopic + "/" + sides[i] + "/position").c_str(), String(100 - sp).c_str());
            client.publish((baseTopic + "/" + sides[i] + "/state").c_str(), coverState(*steppers[i], sp).c_str());
        }
    } else {
        // Single blind: the combined cover.
        long maxPos = getMaxPosition(1);
        // pos runs from -maxPos (fully open) to 0 (fully closed)
        int pct = constrain((int)map(stepper1.currentPosition(), -maxPos, 0, 0, 100), 0, 100);
        client.publish((baseTopic + "/position").c_str(), String(100 - pct).c_str());
        client.publish((baseTopic + "/state").c_str(), coverState(stepper1, pct).c_str());
    }
}

// reconnect to mqtt
void reconnect() {
    if (!client.connected()) {
        // Register a retained Last-Will so HA marks the device offline if the
        // ESP drops off the network unexpectedly.
        if (client.connect(deviceId.c_str(), MQTT_USER, MQTT_PASSWORD,
                           availTopic.c_str(), 0, true, "offline")) {
            client.publish(availTopic.c_str(), "online", true);
            client.subscribe((baseTopic + "/command").c_str());
            client.subscribe((baseTopic + "/set_position").c_str());
            client.subscribe((baseTopic + "/set_pct_closed").c_str());
            client.subscribe((baseTopic + "/set_auto").c_str());
            client.subscribe((baseTopic + "/calibrate").c_str());
            // Per-blind (left / right) control topics
            client.subscribe((baseTopic + "/left/command").c_str());
            client.subscribe((baseTopic + "/left/set_position").c_str());
            client.subscribe((baseTopic + "/right/command").c_str());
            client.subscribe((baseTopic + "/right/set_position").c_str());
            publishDiscovery(); 
            publishState();
            client.publish((baseTopic + "/auto_state").c_str(), cfg_auto_mode ? "ON" : "OFF");
        }
    }
}

// Handle MQTT payloads
void callback(char* topic, byte* payload, unsigned int length) {
    String message;
    for (int i = 0; i < length; i++) message += (char)payload[i];
    String t = String(topic);
    
    if (t.endsWith("/set_auto")) {
        cfg_auto_mode = (message == "ON");
        preferences.putBool("auto", cfg_auto_mode);
        client.publish((baseTopic + "/auto_state").c_str(), cfg_auto_mode ? "ON" : "OFF");
        return;
    }
    if (t.endsWith("/calibrate")) {
        homeRequested = true;
        return;
    }
    
    // Disable auto-mode if someone manually adjusts the blind
    if (cfg_auto_mode) {
        cfg_auto_mode = false; preferences.putBool("auto", false);
        client.publish((baseTopic + "/auto_state").c_str(), "OFF");
    }

    // Which blind does this command target? Left/right sub-topics drive a
    // single motor; the base topics drive both together.
    int which = 0;
    if (t.indexOf("/left/") >= 0) which = 1;
    else if (t.indexOf("/right/") >= 0) which = 2;

    if (t.endsWith("/command")) {
        Serial.println("Recieved MQTT message: " + message);
        if (message == "OPEN") requestBlindMove(0, which);
        else if (message == "CLOSE") requestBlindMove(100, which);
        else if (message == "STOP") {
            // Freeze the targeted motor(s) immediately where they are.
            if (which != 2) { stepper1.stop(); moveRequested = false; }
            if (which != 1 && cfg_motor_count > 1) { stepper2.stop(); moveRequested2 = false; }
            publishState();
        }
    } else if (t.endsWith("/set_position")) {
        requestBlindMove(100 - message.toInt(), which);
    }
    else if (t.endsWith("/set_pct_closed")) {
        requestBlindMove(message.toInt(), which);
    }
}

void mqttSetup() {
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    deviceId = "blind_" + mac.substring(6);
    baseTopic = "home/blinds/" + deviceId;
    availTopic = baseTopic + "/availability";
    Serial.println("mqtt device ID is " + deviceId);

#ifdef USE_MQTT_TLS
    // Accept the broker without CA verification. For full verification call
    // espClient.setCACert(...) with your broker's CA instead.
    espClient.setInsecure();
#endif
    client.setServer(MQTT_SERVER, MQTT_PORT);
    client.setCallback(callback);
    client.setBufferSize(1024);
}

void mqttLoop() {
  if (!client.connected()) {
    static unsigned long lastReconnect = 0;
    if (millis() - lastReconnect > 5000) { 
      lastReconnect = millis(); 
      reconnect(); 
    }
  } 
  else 
  {
    client.loop();
  }

  static unsigned long lastMqttPublish = 0;
  if(millis() - lastMqttPublish > 1000) { 
    publishState(); 
    lastMqttPublish = millis();
  }
}