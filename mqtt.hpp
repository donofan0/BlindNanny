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
    // 1. Cover Entity (controls both blinds together)
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

    // 2. Calibrate Button
    String plCal = "{\"name\":\"Calibrate\","
        "\"unique_id\":\"" + deviceId + "_cal\","
        "\"cmd_t\":\"" + baseTopic + "/calibrate\","
        "\"icon\":\"mdi:arrow-collapse-down\"," + availBlock() + "," + devBlock() + "}";
    client.publish(("homeassistant/button/" + deviceId + "_cal/config").c_str(), plCal.c_str(), true);

    // 3. IP Address Sensor
    String plIP = "{\"name\":\"IP Address\","
        "\"unique_id\":\"" + deviceId + "_ip\","
        "\"stat_t\":\"" + baseTopic + "/ip_address\","
        "\"icon\":\"mdi:ip-network\"," + availBlock() + "," + devBlock() + "}";
    client.publish(("homeassistant/sensor/" + deviceId + "_ip/config").c_str(), plIP.c_str(), true);

    // 4. Slider (percent closed)
    String plSl = "{\"name\":\"Percent Closed\","
        "\"unique_id\":\"" + deviceId + "_pct\","
        "\"cmd_t\":\"" + baseTopic + "/set_pct_closed\","
        "\"stat_t\":\"" + baseTopic + "/pct_closed\","
        "\"min\":0,\"max\":100,\"unit_of_meas\":\"%\","
        "\"icon\":\"mdi:blinds\"," + availBlock() + "," + devBlock() + "}";
    client.publish(("homeassistant/number/" + deviceId + "_pct/config").c_str(), plSl.c_str(), true);

    // 5. Auto Switch
    String plAuto = "{\"name\":\"Auto Sun-Block\","
        "\"unique_id\":\"" + deviceId + "_auto\","
        "\"cmd_t\":\"" + baseTopic + "/set_auto\","
        "\"stat_t\":\"" + baseTopic + "/auto_state\","
        "\"pl_on\":\"ON\",\"pl_off\":\"OFF\","
        "\"icon\":\"mdi:sun-clock\"," + availBlock() + "," + devBlock() + "}";
    client.publish(("homeassistant/switch/" + deviceId + "_auto/config").c_str(), plAuto.c_str(), true);

    // 6+7. Per-blind covers (only when a second motor is fitted) so the left
    // and right blinds can be driven individually from Home Assistant.
    if (cfg_motor_count > 1) {
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
    }
}

// Publish MQTT State
void publishState() {
    if (!client.connected()) return;
    long pos = stepper1.currentPosition();
    long maxPos = getMaxPosition(1);
    // pos runs from -maxPos (fully open) to 0 (fully closed)
    int pct = constrain((int)map(pos, -maxPos, 0, 0, 100), 0, 100);

    client.publish((baseTopic + "/pct_closed").c_str(), String(pct).c_str());
    client.publish((baseTopic + "/position").c_str(), String(100 - pct).c_str());
    client.publish((baseTopic + "/ip_address").c_str(), WiFi.localIP().toString().c_str());

    // Report cover state off the percent-closed value. The old test used the
    // raw (negative) position, so `pos <= 0` was always true and HA saw the
    // cover permanently "open".
    String state = "stopped";
    if(stepper1.isRunning()) state = (stepper1.distanceToGo() > 0) ? "closing" : "opening";
    else if(pct >= 99) state = "closed";
    else if(pct <= 1) state = "open";
    client.publish((baseTopic + "/state").c_str(), state.c_str());

    // Per-blind position/state (left = motor 1, right = motor 2)
    if (cfg_motor_count > 1) {
        AccelStepper* steppers[2] = {&stepper1, &stepper2};
        const char* sides[2] = {"left", "right"};
        for (int i = 0; i < 2; i++) {
            long mp = getMaxPosition(i + 1);
            int sp = constrain((int)map(steppers[i]->currentPosition(), -mp, 0, 0, 100), 0, 100);
            client.publish((baseTopic + "/" + sides[i] + "/position").c_str(),
                           String(100 - sp).c_str());
            String ss = "stopped";
            if(steppers[i]->isRunning()) ss = (steppers[i]->distanceToGo() > 0) ? "closing" : "opening";
            else if(sp >= 99) ss = "closed";
            else if(sp <= 1) ss = "open";
            client.publish((baseTopic + "/" + sides[i] + "/state").c_str(), ss.c_str());
        }
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