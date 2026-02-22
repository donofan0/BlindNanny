#pragma once

#include <login.hpp>

// Announce device to Home Assistant for auto-discovery
void publishDiscovery() {
  // 1. Cover Entity
  String pl = "{\"name\":\"Blind Controller\",\"unique_id\":\"" + deviceId + "\",\"cmd_t\":\"" + baseTopic + "/command\",\"pos_t\":\"" + baseTopic + "/position\",\"set_pos_t\":\"" + baseTopic + "/set_position\",\"stat_t\":\"" + baseTopic + "/state\",\"pl_open\":\"OPEN\",\"pl_cls\":\"CLOSE\",\"pl_stop\":\"STOP\",\"pos_open\":100,\"pos_clsd\":0,\"dev\":{\"ids\":[\"" + deviceId + "\"],\"name\":\"Blind Controller\",\"mf\":\"Seeed\",\"mdl\":\"XIAO C6\"}}";
  client.publish(("homeassistant/cover/" + deviceId + "/config").c_str(), pl.c_str(), true);
  // 2. Calibrate Button
  String plCal = "{\"name\":\"Calibrate Blinds\",\"unique_id\":\"" + deviceId + "_cal\",\"cmd_t\":\"" + baseTopic + "/calibrate\",\"icon\":\"mdi:arrow-collapse-up\",\"dev\":{\"ids\":[\"" + deviceId + "\"]}}";
  client.publish(("homeassistant/button/" + deviceId + "_cal/config").c_str(), plCal.c_str(), true);
  // 3. IP Address Sensor
  String plIP = "{\"name\":\"Blind IP Address\",\"unique_id\":\"" + deviceId + "_ip\",\"stat_t\":\"" + baseTopic + "/ip_address\",\"icon\":\"mdi:ip-network\",\"dev\":{\"ids\":[\"" + deviceId + "\"]}}";
  client.publish(("homeassistant/sensor/" + deviceId + "_ip/config").c_str(), plIP.c_str(), true);
  // 4. Slider
  String plSl = "{\"name\":\"Blind % Closed\",\"unique_id\":\"" + deviceId + "_pct\",\"cmd_t\":\"" + baseTopic + "/set_pct_closed\",\"stat_t\":\"" + baseTopic + "/pct_closed\",\"min\":0,\"max\":100,\"icon\":\"mdi:blinds\",\"dev\":{\"ids\":[\"" + deviceId + "\"]}}";
  client.publish(("homeassistant/number/" + deviceId + "_pct/config").c_str(), plSl.c_str(), true);
  // 5. Auto Switch
  String plAuto = "{\"name\":\"Blind Auto Mode\",\"unique_id\":\"" + deviceId + "_auto\",\"cmd_t\":\"" + baseTopic + "/set_auto\",\"stat_t\":\"" + baseTopic + "/auto_state\",\"pl_on\":\"ON\",\"pl_off\":\"OFF\",\"icon\":\"mdi:sun-clock\",\"dev\":{\"ids\":[\"" + deviceId + "\"]}}";
  client.publish(("homeassistant/switch/" + deviceId + "_auto/config").c_str(), plAuto.c_str(), true);
}

// Publish MQTT State
void publishState() {
    if (!client.connected()) return;
    long pos = stepper1.currentPosition();
    long maxPos = getMaxPosition(1);
    int pct = map(pos, 0, maxPos, 0, 100);
    pct = constrain(pct, 0, 100);
    float m = ((float)pct / 100.0f) * cfg_m1_max_meters;
    
    client.publish((baseTopic + "/pct_closed").c_str(), String(pct).c_str());
    client.publish((baseTopic + "/meters_closed").c_str(), String(m, 2).c_str());
    client.publish((baseTopic + "/position").c_str(), String(100 - pct).c_str());
    client.publish((baseTopic + "/ip_address").c_str(), WiFi.localIP().toString().c_str());
    
    String state = "stopped";
    if(stepper1.isRunning()) state = (stepper1.speed() > 0) ? "closing" : "opening";
    else if(pos <= 0) state = "open";
    else if(pos >= maxPos) state = "closed";
    client.publish((baseTopic + "/state").c_str(), state.c_str());
}

// reconnect to mqtt
void reconnect() {
    if (!client.connected()) {
        if (client.connect(deviceId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
            client.subscribe((baseTopic + "/command").c_str());
            client.subscribe((baseTopic + "/set_position").c_str());
            client.subscribe((baseTopic + "/set_pct_closed").c_str());
            client.subscribe((baseTopic + "/set_auto").c_str());
            client.subscribe((baseTopic + "/calibrate").c_str());
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

    if (t.endsWith("/command")) {
        if (message == "OPEN") { moveTarget = getMaxPosition(1); moveRequested = true; }
        else if (message == "CLOSE") { moveTarget = 0; moveRequested = true; }
        else if (message == "STOP") { 
            // Freeze motor 1 immediately where it is
            stepper1.stop();
            moveTarget = -stepper1.currentPosition();
            stepper1.moveTo(moveTarget); 
            
            // Freeze motor 2 immediately where it is
            if (cfg_motor_count > 1) 
            {
                stepper2.stop();
                stepper2.moveTo(-stepper2.currentPosition());
            }
            publishState();
        } 
    } else if (t.endsWith("/set_position")) {
        moveTarget = pctToSteps(100 - message.toInt());
        moveRequested = true;
    } else if (t.endsWith("/set_pct_closed")) {
        moveTarget = pctToSteps(message.toInt());
        moveRequested = true;
    }
}

void mqttSetup() {
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    deviceId = "blind_" + mac.substring(6);
    baseTopic = "home/blinds/" + deviceId; 
    Serial.println("mqtt device ID is " + deviceId);

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