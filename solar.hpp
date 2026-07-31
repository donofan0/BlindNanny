#pragma once

// --- SUN TRACKING ---
// Helper to just get current sun position and target without side effects.
//
// Uses the NOAA solar-position algorithm computed off UTC. Working in UTC and
// deriving true solar time from longitude + the equation of time means the
// result is independent of the configured timezone AND of daylight saving.
// The old code read the local wall clock (configTime + fixed cfg_gmt_offset,
// which has no DST), so every summer the sun angle was ~1 hour out and tracking
// drifted badly toward sunset. This version needs only cfg_lat / cfg_lon.
int calculateSunPosition(float &azOut, float &elOut) {
    time_t now;
    time(&now);
    struct tm tUtc;
    gmtime_r(&now, &tUtc);                 // UTC, not local -> DST-proof
    int dayOfYear = tUtc.tm_yday;          // 0 = Jan 1
    float utcHour = tUtc.tm_hour + (tUtc.tm_min / 60.0) + (tUtc.tm_sec / 3600.0);

    // Fractional year (radians)
    float gamma = (TWO_PI / 365.0) * (dayOfYear + (utcHour - 12.0) / 24.0);

    // Equation of time (minutes) and solar declination (radians) - NOAA/Spencer
    float eqTime = 229.18 * (0.000075 + 0.001868 * cos(gamma) - 0.032077 * sin(gamma)
                   - 0.014615 * cos(2 * gamma) - 0.040849 * sin(2 * gamma));
    float declination = 0.006918 - 0.399912 * cos(gamma) + 0.070257 * sin(gamma)
                        - 0.006758 * cos(2 * gamma) + 0.000907 * sin(2 * gamma)
                        - 0.002697 * cos(3 * gamma) + 0.00148 * sin(3 * gamma);

    // True solar time (minutes). Longitude east positive; timezone term is 0
    // because utcHour is already UTC.
    float trueSolarTime = utcHour * 60.0 + eqTime + 4.0 * cfg_lon;
    float hourAngleDeg = (trueSolarTime / 4.0) - 180.0;   // -180..+180, 0 at solar noon
    float hourAngle = hourAngleDeg * DEG_TO_RAD;

    float latRad = cfg_lat * DEG_TO_RAD;
    float sinEl = sin(latRad) * sin(declination) + cos(latRad) * cos(declination) * cos(hourAngle);
    float elRad = asin(sinEl);
    float elDeg = elRad * RAD_TO_DEG;
    float cosAz = (sin(declination) - sinEl * sin(latRad)) / (cos(elRad) * cos(latRad));

    if(cosAz > 1.0) cosAz = 1.0;
    if(cosAz < -1.0) cosAz = -1.0;

    float azRad = acos(cosAz);
    float azDeg = azRad * RAD_TO_DEG;         // measured from North (0=N, 180=S)
    if (hourAngleDeg > 0) azDeg = 360.0 - azDeg;  // afternoon -> sun in the west

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
    int targetPct = calculateSunPosition(az, el);   // percent closed needed to block glare

    // Blind 1 (left), in M1's own scale, with a small deadband to avoid jitter.
    long steps1 = pctToStepsMotor(targetPct, 1);
    if (abs(steps1 + stepper1.currentPosition()) > (getMaxPosition(1) / 50)) {
        moveTarget = steps1;
        moveRequested = true;
    }
    // Blind 2 (right) must track too when fitted - previously it was left
    // stationary in auto mode while only the left blind followed the sun.
    if (cfg_motor_count > 1) {
        long steps2 = pctToStepsMotor(targetPct, 2);
        if (abs(steps2 + stepper2.currentPosition()) > (getMaxPosition(2) / 50)) {
            moveTarget2 = steps2;
            moveRequested2 = true;
        }
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
