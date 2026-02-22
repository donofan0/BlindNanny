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

// pctToSteps maps to Motor 1 space (the primary reference)
long pctToSteps(int pct) { return map(pct, 100, 0, 0, getMaxPosition(1)); }

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
    Serial.printf("Homing Blind %d with stall sensitivity %d/255...\n", id, cfg_m1_stall);
    digitalWrite((id==1 ? EN1_PIN : EN2_PIN), LOW);
    s->stop(); 
    
    attachInterrupt(digitalPinToInterrupt(diag), isr, RISING);
    digitalWrite(dir, HIGH); // Moving 'up' against the hard stop
    
    *stalled = false;
    unsigned long start = millis();
    while (!(*stalled) && ( (millis() - start) < (HOMEING_TIMEOUT_SECS * 1000) )) {
        digitalWrite(step, HIGH);
        delayMicroseconds(200); 
        digitalWrite(step, LOW);
        delayMicroseconds(200);
        yield(); 
    }
    
    detachInterrupt(digitalPinToInterrupt(diag));
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
  }

  homeBlind(1);
  homeBlind(2);
}

void blindLoop()
{
  // Process async homing
  if (homeRequested) { 
    homeBlind(1); 
    homeBlind(2);
    homeRequested = false; 
  }
  
  // Process new movement positions safely
  if (moveRequested) { 
    moveRequested = false;
    enableMotors(true);
    stepper1.moveTo(-moveTarget);
    Serial.printf("Moving blind 1 to %ld\n", moveTarget);
    if (cfg_motor_count > 1) {
      // Because max bounds might be different, map M1's target steps 
      // proportionally to M2's total steps based on independent sizing
      long max1 = getMaxPosition(1);
      long max2 = getMaxPosition(2);
      long target2 = (max1 == 0) ? 0 : (moveTarget * ((float)max2 / max1));
      stepper2.moveTo(-target2);
      Serial.printf("Moving blind 2 to %ld since multiple is %ld/%ld\n", target2, max2, max1);
    }
  }

  stepper1.run();
  if (cfg_motor_count > 1) {
    stepper2.run();
  }
  
  // --- POWER DOWN LOGIC ---
  // Check if either enabled motor is running to prevent overheating
  static unsigned long lastMoveTime = 0;
  if (stepper1.isRunning() || ( (cfg_motor_count > 1) && stepper2.isRunning() )) {
    lastMoveTime = millis();
  } else {
    // If stopped for > 2 seconds, disable motors entirely
    if (motorsEnabled && (millis() - lastMoveTime > 2000)) {
      enableMotors(false);
    }
  }
}