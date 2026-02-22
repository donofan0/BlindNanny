#pragma once

// --- SUN TRACKING ---
// Helper to just get current sun position and target without side effects
int calculateSunPosition(float &azOut, float &elOut) {
    time_t now;
    time(&now); struct tm * t = localtime(&now);
    int dayOfYear = t->tm_yday;
    float hour = t->tm_hour + (t->tm_min / 60.0);
    
    // Complex astronomical formula logic
    float declination = 23.45 * sin(TWO_PI * (284 + dayOfYear) / 365.0) * DEG_TO_RAD;
    float hourAngle = (hour - 12.0) * 15.0 * DEG_TO_RAD;
    float latRad = cfg_lat * DEG_TO_RAD;
    float sinEl = sin(latRad) * sin(declination) + cos(latRad) * cos(declination) * cos(hourAngle);
    float elRad = asin(sinEl);
    float elDeg = elRad * RAD_TO_DEG;
    float cosAz = (sin(declination) - sinEl * sin(latRad)) / (cos(elRad) * cos(latRad));
    
    if(cosAz > 1.0) cosAz = 1.0;
    if(cosAz < -1.0) cosAz = -1.0;
    
    float azRad = acos(cosAz);
    float azDeg = azRad * RAD_TO_DEG;
    if (hour > 12) azDeg = 360.0 - azDeg;

    azOut = azDeg;
    elOut = elDeg;

    int targetPct = 0;
    if (elDeg > 0) {
        float winAz = (float)cfg_win_az;
        float gamma = fabs(azDeg - winAz);
        if (gamma > 180) gamma = 360 - gamma;
        
        // If the sun is in front of the window
        if (gamma < 90) {
            float tanP = tan(elRad) / cos(gamma * DEG_TO_RAD);
            if(tanP < 0) tanP = 0;
            float hShadow = cfg_eye_h + (cfg_user_dist * tanP);
            float extensionMeters = cfg_win_top - hShadow;
            
            // Limit extension strictly to M1's maximum allowed height
            if (extensionMeters < 0) extensionMeters = 0; 
            if (extensionMeters > cfg_m1_max_meters) extensionMeters = cfg_m1_max_meters;
            targetPct = (int)((extensionMeters / cfg_m1_max_meters) * 100);
        }
    }
    return targetPct;
}

// Calculate updates based on sun path
void updateSunTracking() {
    float az, el;
    int targetPct = calculateSunPosition(az, el);
    // Apply to motors natively based off Motor 1 scale
    long steps = pctToSteps(targetPct);
    long maxPos = getMaxPosition(1);
    
    if (abs(steps - stepper1.currentPosition()) > (maxPos/50)) {
         moveTarget = steps;
         moveRequested = true;
    }
}

void solarLoop()
{
  static unsigned long lastSunCheck = 0;

  if (cfg_auto_mode && !motorsEnabled && (millis() - lastSunCheck > 10000)) {
      updateSunTracking();
      lastSunCheck = millis();
  }
}
