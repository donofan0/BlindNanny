#pragma once

#include <html.hpp>
#include <solar.hpp>

void webServerSetup() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ request->send_P(200, "text/html", index_html); });
    
    server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request){
        cfg_auto_mode = false; preferences.putBool("auto", false);
        int pct = request->hasParam("pos") ? request->getParam("pos")->value().toInt() : 0;
        moveTarget = pctToSteps(pct); moveRequested = true;
        request->send(200, "text/plain", "OK");
    });
    
    server.on("/home", HTTP_GET, [](AsyncWebServerRequest *request){ 
        homeRequested = true; 
        Serial.println("Homeing requested");
        request->send(200, "text/plain", "Homing"); 
    });
    
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
        long maxPos = getMaxPosition(1);
        int p = map(stepper1.currentPosition(), -maxPos, 0, 0, 100);
        String j = "{\"pos\":" + String(p) + ", \"auto\":" + (cfg_auto_mode?"1":"0") + "}";
        request->send(200, "application/json", j);
    });
    
    server.on("/diag", HTTP_GET, [](AsyncWebServerRequest *request){
        float az, el;
        int targ = calculateSunPosition(az, el);
        String j = "{";
        j += "\"uptime\":" + String(millis()) + ",";
        j += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
        j += "\"rssi\":" + String(WiFi.RSSI()) + ",";
        j += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
        j += "\"mac\":\"" + WiFi.macAddress() + "\",";
        j += "\"mqtt\":" + String(client.connected() ? "true" : "false") + ",";
        j += "\"enabled\":" + String(motorsEnabled ? "true" : "false") + ",";
        j += "\"m1\":{\"pos\":" + String(stepper1.currentPosition()) + ",\"current\":" + String(driver1.cs2rms(driver1.cs_actual())) + "},";
        j += "\"m2\":{\"pos\":" + String(stepper2.currentPosition()) + ",\"current\":" + String(cfg_motor_count > 1 ? driver2.cs2rms(driver2.cs_actual()) : 0) + "},";
        j += "\"sun\":{\"az\":" + String(az) + ",\"el\":" + String(el) + ",\"targ\":" + String(targ) + "}";
        j += "}";
        request->send(200, "application/json", j);
    });
    
    server.on("/save_cfg", HTTP_GET, [](AsyncWebServerRequest *request){
        if(request->hasParam("lat")) cfg_lat = request->getParam("lat")->value().toFloat();
        if(request->hasParam("lon")) cfg_lon = request->getParam("lon")->value().toFloat();
        if(request->hasParam("az")) cfg_win_az = request->getParam("az")->value().toInt();
        if(request->hasParam("top")) cfg_win_top = request->getParam("top")->value().toFloat();
        if(request->hasParam("eye")) cfg_eye_h = request->getParam("eye")->value().toFloat();
        if(request->hasParam("dist")) cfg_user_dist = request->getParam("dist")->value().toFloat();
        if(request->hasParam("gmt")) cfg_gmt_offset = request->getParam("gmt")->value().toFloat();
        if(request->hasParam("auto")) cfg_auto_mode = (request->getParam("auto")->value().toInt() == 1);
        
        // Grab independent sizes and configurations
        if(request->hasParam("m1_max")) cfg_m1_max_meters = request->getParam("m1_max")->value().toFloat();
        if(request->hasParam("m2_max")) cfg_m2_max_meters = request->getParam("m2_max")->value().toFloat();
        if(request->hasParam("spcm")) cfg_steps_per_cm = request->getParam("spcm")->value().toFloat();
        if(request->hasParam("cnt")) cfg_motor_count = request->getParam("cnt")->value().toInt();
        
        if(request->hasParam("speed")) cfg_speed = request->getParam("speed")->value().toInt();
        if(request->hasParam("m1_curr")) cfg_m1_current = request->getParam("m1_curr")->value().toInt();
        if(request->hasParam("m2_curr")) cfg_m2_current = request->getParam("m2_curr")->value().toInt();
        if(request->hasParam("m1_stall")) cfg_m1_stall = request->getParam("m1_stall")->value().toInt();
        if(request->hasParam("m2_stall")) cfg_m2_stall = request->getParam("m2_stall")->value().toInt();

        // Write to flash
        preferences.putFloat("lat", cfg_lat); preferences.putFloat("lon", cfg_lon); preferences.putInt("az", cfg_win_az);
        preferences.putFloat("top", cfg_win_top); preferences.putFloat("eye", cfg_eye_h); preferences.putFloat("dist", cfg_user_dist);
        preferences.putFloat("gmt", cfg_gmt_offset); preferences.putBool("auto", cfg_auto_mode);
        
        preferences.putFloat("m1_max", cfg_m1_max_meters);
        preferences.putFloat("m2_max", cfg_m2_max_meters);
        preferences.putFloat("spcm", cfg_steps_per_cm);
        preferences.putInt("cnt", cfg_motor_count);
        preferences.putInt("speed", cfg_speed);
        
        preferences.putInt("m1_curr", cfg_m1_current);
        preferences.putInt("m2_curr", cfg_m2_current);
        preferences.putInt("m1_stall", cfg_m1_stall);
        preferences.putInt("m2_stall", cfg_m2_stall);

        // Apply Immediate Motor Changes
        stepper1.setMaxSpeed(cfg_speed); 
        if(cfg_motor_count > 1) stepper2.setMaxSpeed(cfg_speed);
        
        // Apply individual currents and stalls immediately
        driver1.rms_current(cfg_m1_current); driver1.SGTHRS(cfg_m1_stall);
        if(cfg_motor_count > 1) { 
            driver2.rms_current(cfg_m2_current); 
            driver2.SGTHRS(cfg_m2_stall);
        }

        configTime(cfg_gmt_offset * 3600, 0, "pool.ntp.org");
        Serial.println("Saved new config");
        request->send(200, "text/plain", "Saved");
    });
    
    server.on("/get_cfg", HTTP_GET, [](AsyncWebServerRequest *request){
        String j = "{"; 
        j += "\"lat\":" + String(cfg_lat, 4) + ","; 
        j += "\"lon\":" + String(cfg_lon, 4) + ","; 
        j += "\"az\":" + String(cfg_win_az) + ",";
        j += "\"top\":" + String(cfg_win_top, 2) + ","; 
        j += "\"eye\":" + String(cfg_eye_h, 2) + ","; 
        j += "\"dist\":" + String(cfg_user_dist, 2) + ",";
        j += "\"gmt\":" + String(cfg_gmt_offset, 1) + ","; 
        j += "\"auto\":" + String(cfg_auto_mode ? 1 : 0) + ",";
        
        j += "\"m1_max\":" + String(cfg_m1_max_meters, 2) + ","; 
        j += "\"m2_max\":" + String(cfg_m2_max_meters, 2) + ","; 
        j += "\"spcm\":" + String(cfg_steps_per_cm, 2) + ",";
        j += "\"cnt\":" + String(cfg_motor_count) + ",";
        j += "\"speed\":" + String(cfg_speed) + ",";
        
        j += "\"m1_curr\":" + String(cfg_m1_current) + ",";
        j += "\"m2_curr\":" + String(cfg_m2_current) + ",";
        j += "\"m1_stall\":" + String(cfg_m1_stall) + ",";
        j += "\"m2_stall\":" + String(cfg_m2_stall);
        j += "}";
        request->send(200, "application/json", j);
    });
    
    server.begin();
}
