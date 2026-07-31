#pragma once

volatile bool stalled1 = false;
volatile bool stalled2 = false;

// --- DIAGNOSTIC INTERRUPTS ---
void IRAM_ATTR onDiag1() { stalled1 = true; }
void IRAM_ATTR onDiag2() { stalled2 = true; }

// Return max steps based strictly on each motor's independent configured height
long getMaxPosition(int motorId) {
    long max = (long)(cfg_steps_per_cm * cfg_m1_max_meters * 100);
    if (motorId == 2) {
      max = (long)(cfg_steps_per_cm * cfg_m2_max_meters * 100);
    }
    // Serial.printf("Max postition for motor %d is %d\n", motorId, max);
    return max;
}

// pctToStepsMotor maps a percent-closed value into a given motor's own step
// space (each motor can have a different configured height).
long pctToStepsMotor(int pct, int motorId) { return map(pct, 100, 0, 0, getMaxPosition(motorId)); }

// pctToSteps maps to Motor 1 space (the primary reference)
long pctToSteps(int pct) { return pctToStepsMotor(pct, 1); }

// Request a move by percent-closed (0 = open, 100 = closed).
//   which = 0 -> both blinds, 1 -> left (motor 1) only, 2 -> right (motor 2) only
// Each motor is targeted in its own scale so mismatched heights stay aligned.
void requestBlindMove(int pctClosed, int which) {
  pctClosed = constrain(pctClosed, 0, 100);
  if (which != 2) {
    moveTarget = pctToStepsMotor(pctClosed, 1);
    moveRequested = true;
  }
  if (which != 1 && cfg_motor_count > 1) {
    moveTarget2 = pctToStepsMotor(pctClosed, 2);
    moveRequested2 = true;
  }
}

// Persist both blind positions to flash so they survive a power cut and the
// device doesn't have to run a slow, noisy re-home on every boot.
void savePositions() {
  preferences.putLong("pos1", stepper1.currentPosition());
  preferences.putLong("pos2", stepper2.currentPosition());
  preferences.putBool("pos_valid", true);
}

// --- POWER MANAGEMENT ---
// Enable or disable driver power to reduce heat
void enableMotors(bool enabled) {
  digitalWrite(EN1_PIN, enabled ? LOW : HIGH); // LOW = Enable
  if (cfg_motor_count > 1) {
    digitalWrite(EN2_PIN, enabled ? LOW : HIGH);
  }
  motorsEnabled = enabled;
  if(enabled) {
    delay(5); // brief delay to allow drivers to wake
  }
}


// Auto-Homing sequence utilizing StallGuard
void homeBlind(int id) {
    if (id == 2 && cfg_motor_count < 2) {
      return;
    }
    AccelStepper* s = (id==1) ? &stepper1 : &stepper2;
    int diag = (id==1) ? DIAG1_PIN : DIAG2_PIN;
    int step = (id==1) ? STEP1_PIN : STEP2_PIN;
    int dir  = (id==1) ? DIR1_PIN : DIR2_PIN;
    void (*isr)() = (id==1) ? onDiag1 : onDiag2;
    volatile bool* stalled = (id==1) ? &stalled1 : &stalled2;
    
    int stall_val = id == 1 ? cfg_m1_stall : cfg_m2_stall;
    bool invertDir = (id==1) ? cfg_m1_invert : cfg_m2_invert;
    Serial.printf("Homing Blind %d with stall sensitivity %d/255...\n", id, stall_val);
    digitalWrite((id==1 ? EN1_PIN : EN2_PIN), LOW);
    s->stop();

    attachInterrupt(digitalPinToInterrupt(diag), isr, RISING);
    digitalWrite(dir, invertDir ? LOW : HIGH); // drive toward the hard stop

    *stalled = false;
    unsigned long start = millis();
    unsigned long steps = 0;
    while (!(*stalled) && ( (millis() - start) < (HOMEING_TIMEOUT_SECS * 1000) )) {
        digitalWrite(step, HIGH);
        delayMicroseconds(200);
        digitalWrite(step, LOW);
        delayMicroseconds(200);
        yield();
        // Keep WiFi/MQTT alive during this long blocking loop so the broker
        // connection doesn't drop mid-calibration.
        if ((++steps & 0x3FF) == 0) client.loop();
    }

    detachInterrupt(digitalPinToInterrupt(diag));

    // Relieve belt tension by backing a little off the hard stop, then zero
    // the position there so a full-close move never slams the stop.
    if (*stalled) {
        digitalWrite(dir, invertDir ? HIGH : LOW); // reverse away from the stop
        for (int i = 0; i < HOME_BACKOFF_STEPS; i++) {
            digitalWrite(step, HIGH);
            delayMicroseconds(200);
            digitalWrite(step, LOW);
            delayMicroseconds(200);
            yield();
        }
    }
    s->setCurrentPosition(0); // Reset position locally based on individual constraints
    digitalWrite((id==1 ? EN1_PIN : EN2_PIN), HIGH);

    if (!*stalled) {
      Serial.printf("Blind %d Homing Timed Out!\n", id);
    }
    else
    {
      Serial.printf("Blind %d Homed successfully in %d seconds\n", id, (millis() - start)/1000);
    }
}

void initDriver(TMC2209Stepper &d, int stall_val, int curr_val) {
  d.begin();
  d.toff(5);
  d.rms_current(curr_val); // Assigned explicitly per driver
  d.microsteps(16);
  d.TCOOLTHRS(0xFFFFF);
  d.SGTHRS(stall_val);     // Assigned explicitly per driver
}

void blindSetup()
{
  // PIN SETUP
  pinMode(DIR1_PIN, OUTPUT); pinMode(STEP1_PIN, OUTPUT); pinMode(EN1_PIN, OUTPUT); pinMode(DIAG1_PIN, INPUT_PULLUP);
  pinMode(DIR2_PIN, OUTPUT); pinMode(STEP2_PIN, OUTPUT); pinMode(EN2_PIN, OUTPUT); pinMode(DIAG2_PIN, INPUT_PULLUP);
  
  enableMotors(false);
  
  TMC_SERIAL_PORT.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
  
  // driver 1 init
  initDriver(driver1, cfg_m1_stall, cfg_m1_current);
  uint8_t v1 = driver1.version();
  if (v1 == 0 || v1 == 0xFF) {
    Serial.println("Warning: Driver 1 not detected.");
  }
  else
  {
    Serial.println("Driver 1 is online");
  }
  stepper1.setMaxSpeed(cfg_speed);
  stepper1.setAcceleration(ACCELERATION);
  stepper1.setPinsInverted(cfg_m1_invert, false, false);

  // Conditional Driver 2 Init
  if (cfg_motor_count > 1) {
    initDriver(driver2, cfg_m2_stall, cfg_m2_current);
    uint8_t v2 = driver2.version();
    if (v2 == 0 || v2 == 0xFF) {
      Serial.println("Warning: Driver 2 not detected.");
    }
    else
    {
      Serial.println("Driver 2 is online");
    }
    stepper2.setMaxSpeed(cfg_speed);
    stepper2.setAcceleration(ACCELERATION);
    stepper2.setPinsInverted(cfg_m2_invert, false, false);
  }

  // Restore the last known positions instead of blocking on a boot-time home.
  // Homing stays available on demand via the Calibrate command.
  if (preferences.getBool("pos_valid", false)) {
    stepper1.setCurrentPosition(preferences.getLong("pos1", 0));
    stepper2.setCurrentPosition(preferences.getLong("pos2", 0));
    Serial.println("Restored saved blind positions; skipping boot homing.");
  } else {
    homeBlind(1);
    homeBlind(2);
    savePositions();
  }
}

void blindLoop()
{
  // Process async homing
  if (homeRequested) {
    homeBlind(1);
    homeBlind(2);
    homeRequested = false;
    savePositions();
  }
  
  // Process new movement positions safely. Blind 1 (left) and blind 2 (right)
  // are handled independently so they can be driven together or one at a time.
  if (moveRequested) {
    moveRequested = false;
    enableMotors(true);
    stepper1.moveTo(-moveTarget);
    Serial.printf("Moving blind 1 to %ld\n", moveTarget);
  }
  if (moveRequested2 && cfg_motor_count > 1) {
    moveRequested2 = false;
    enableMotors(true);
    stepper2.moveTo(-moveTarget2);
    Serial.printf("Moving blind 2 to %ld\n", moveTarget2);
  }

  stepper1.run();
  if (cfg_motor_count > 1) {
    stepper2.run();
  }
  
  // --- POWER DOWN LOGIC ---
  // Check if either enabled motor is running to prevent overheating
  static unsigned long lastMoveTime = 0;
  static bool posDirty = false;
  if (stepper1.isRunning() || ( (cfg_motor_count > 1) && stepper2.isRunning() )) {
    lastMoveTime = millis();
    posDirty = true;              // a move is in progress; positions changed
  } else {
    // If stopped for > 2 seconds, disable motors entirely
    if (motorsEnabled && (millis() - lastMoveTime > 2000)) {
      enableMotors(false);
    }
    // Persist the final position once, after the blind has settled.
    if (posDirty && (millis() - lastMoveTime > 2000)) {
      savePositions();
      posDirty = false;
    }
  }
}